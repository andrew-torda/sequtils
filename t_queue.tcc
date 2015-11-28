/*
 * 21 Nov 2015
 * This is a wrapper around the STL queue which is thread safe.
 * There are
 * - minimal locks when pushing and popping in the STL queue
 * - an atomic counter for the size of the queue
 * - local buffering - we do not directly put things in the queue, since
 *   this leads to lots of contention. Instead we save them in local vectors.
 *   When they are full/empty, we flush them or refill them.
 * - throttling. You can say that the filler/producer should wait if the queue
 *   has too many members. It will be woken up again when the number of items
 *   goes below min_buf.
 *
 * At the moment, we have one size for the in/out buffers.
 */
#ifndef T_QUEUE_TCC
#define T_QUEUE_TCC 1
#include <atomic>
#include <condition_variable>
#include <queue>
#include <vector>


#include <string>  /* only during debugging */
#include "t_queue.hh"
#include "delay.hh"
static void breaker2() {}

static const unsigned char NOTHING      = 0;
static const unsigned char CLOSED       = 0x1;
static const unsigned char ALIVE_WAITING= 0x2;

/* ---------------- constructors  ----------------------------
 */

template <typename T>
void
t_queue<T>::init() {
    //    alive_wants_notification = false;
    q_state = NOTHING;
    throttled = false;
    atm_size = 0;
    consum_cnt = 0;
    feed_cnt = 0;
    c_buf_was_empty = true;
    feed_buf_full = false;
}

template <typename T>
t_queue<T>::t_queue() {
    do_throttling = false;
    max_buf = 1;
    init();
}

template <typename T>   /* Called with internal buffering */
t_queue<T>::t_queue (short unsigned intern_buf_siz) {
    max_buf = intern_buf_siz;
    do_throttling = false;
    feed_buf.reserve(intern_buf_siz);
    consum_buf.reserve(intern_buf_siz);
    init();
}


template <typename T>
t_queue<T>::t_queue (unsigned min_q, unsigned max_q) {
    min_in_q = min_q;
    max_in_q = max_q;
    max_buf = 1;
    do_throttling = false;
    init();
}

template <typename T>
t_queue<T>::t_queue (short unsigned intern_buf_siz, unsigned min_q, unsigned max_q) {    
    min_in_q = min_q;
    max_in_q = max_q;
    max_buf = intern_buf_siz;
    do_throttling = true;
    init();
    if (max_in_q <= min_in_q)
        exit(1);  /* This should throw a bad parameter error */
}

/* ---------------- flush   ----------------------------------
 * Flush the buffer for the producer / feeder.
 */
template <typename T>
void
t_queue<T>::flush()
{
    unsigned char n = 0;
    {
        std::lock_guard<std::mutex> lock (q_mtx);
        for (typename std::vector <T>::const_iterator it = feed_buf.begin() ; it != feed_buf.end(); ++it)
            p_q.push (*it);
    }
    if (feed_buf.size())
        atomic_fetch_add (&atm_size, feed_buf.size());

    if (q_state & ALIVE_WAITING) {
        std::lock_guard<std::mutex> refill_lock(refill_mtx);
        need_refill.notify_one();
    }

    feed_buf.clear();
}

/* ---------------- push    ----------------------------------
 */
template <typename T>
void
t_queue<T>::push(const T t)
{
    feed_buf.push_back (t);
    if (feed_buf.size() == max_buf)
        flush();

    if (do_throttling) {
        if (atm_size >= max_in_q) {
            std::unique_lock<std::mutex> throttle_lock(throttle_mtx);
            throttled = true;
            throttled_wait.wait (throttle_lock);
        }
        throttled = false;
    }
}

/* ---------------- close  -----------------------------------
 * Close the queue and tell anybody who might be waiting that
 * they should look for their last data.
 */
template <typename T>
void
t_queue<T>::close()
{
    flush();
    q_state |= CLOSED;
    std::lock_guard<std::mutex> refill_lock(refill_mtx);
    need_refill.notify_one(); /* say we are really finished */
}

/* ---------------- unthrottle -------------------------------
 * Any time the consumer works on the queue, he might want to
 * check if the queue has been throttled, so we put it into
 * a convenient function.
 */
template <typename T>
bool
t_queue<T>::unthrottle()
{
    if (do_throttling) {
        if (throttled && (atm_size <= min_in_q)) {
            std::lock_guard<std::mutex> throttle_lock(throttle_mtx);
            throttled_wait.notify_one();
        }
    }
}

/* ---------------- alive  -----------------------------------
 * This tells us if the queue is still alive. It does the
 * waiting if it is empty.
 * If the queue is empty and it is closed and the local buffer
 * is empty, than we are really fished and return false.
 */
template <typename T>
bool
t_queue<T>::alive ()
{
    if ( !(c_buf_was_empty) || (atm_size != 0)) /* most common case */
        return true;

    if ( (q_state & CLOSED ) && c_buf_was_empty )
        return false;

 /* Reaching here means the queue is not closed, but the buffer is empty */

    while ((atm_size == 0) &&  !(q_state & CLOSED)) {
        if (do_throttling) {
            std::lock_guard<std::mutex> throttle_lock(throttle_mtx);
            if (throttled)
                throttled_wait.notify_one();
        }

        std::unique_lock<std::mutex> refill_lock(refill_mtx);
        q_state |= ALIVE_WAITING;
        need_refill.notify_one();
    }
    q_state &= ~ALIVE_WAITING;
    
    /* We have waited, but it could be that the queue simply closed */
    if (atm_size == 0) {
        std::cerr << __func__ << " ending with atm_size = 0\n";
        return false;
    }
    return true;
}

/* ---------------- front_and_pop ----------------------------
 * We buffer reader from the queue to avoid fighting for the
 * lock. We buffer up to max_buf elements.
 * We have called alive() previously, so we do not have to wait
 * on a refill.   
 * If I combine front() and pop(), I only have to lock the queue once.
 * Unfortunately, I cannot get it to work if I use a reference to the
 * returned T. This is not tragic, but means I am copying unnecessarily.
 * Why do we use a separate variable to say if the local buffer was empty ?
 * We could look at the atomic state variable, but that means locking with
 * other processes. The only place the variable can change is here, so we
 * store a copy in c_buf_was empty. We use the atomic variable to tell
 * other processes that our buffer is empty. This is used when we are
 * deciding if we are finished or not.
 * Similarly, we may have to make two changes if we mark our buffer as
 * empty and the whole queue as empty. We save the changes in a local variable
 * and only access the atomic variable once.
 */
template <typename T>
const T
t_queue<T>::front_and_pop()
{
    T tmp;
    unsigned char to_change = 0;
    typename std::queue<T>::size_type to_get = 0;
    if (c_buf_was_empty) {
        consum_buf.clear();
        to_get = atm_size;              /* How many items  */
        if (max_buf < to_get)           /* should we get from the queue ? */
            to_get = max_buf;
        consum_buf.reserve(to_get);
        {
            std::lock_guard<std::mutex> lock (q_mtx);
            for (unsigned i = 0; i < to_get; i++) {
                consum_buf.push_back(p_q.front());
                p_q.pop();
            }
        }
        if (to_get) {
            c_buf_was_empty = false;
            atomic_fetch_sub (&atm_size, to_get);
        }
        consum_cnt = 0;
        unthrottle();
    }

    tmp = consum_buf [ consum_cnt++];

    if (consum_cnt == consum_buf.size())
        c_buf_was_empty = true;

    return tmp;
}

#endif /* T_QUEUE_TCC */
