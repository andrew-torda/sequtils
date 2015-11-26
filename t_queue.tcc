/*
 * 21 Nov 2015
 */
#ifndef T_QUEUE_TCC
#define T_QUEUE_TCC 1
#include <atomic>
#include <condition_variable>
#include <queue>
#include <vector>

#include "t_queue.hh"

static const unsigned char NOTHING      = 0;
static const unsigned char Q_EMPTY      = 0x1;
static const unsigned char CLOSED       = 0x2;
static const unsigned char C_BUF_EMPTY  = 0x4;

/* ---------------- constructors  ----------------------------
 */

template <typename T>
void
t_queue<T>::init() {
    alive_wants_notification = false;
    empty_closed = C_BUF_EMPTY;
    throttled = false;
    atm_size = 0;
    consum_cnt = 0;
    feed_cnt = 0;
    c_buf_was_empty = true;
    do_throttling = false;
}

template <typename T>
t_queue<T>::t_queue() {
    do_throttling = false;
    init();
}

template <typename T>
t_queue<T>::t_queue (unsigned min_q, unsigned max_q) {
    min_in_q = min_q;
    max_in_q = max_q;
    do_throttling = true;
    init();
}

template <typename T>
t_queue<T>::t_queue (short unsigned prod_buf_siz, short unsigned consum_buf_size,
                  unsigned min_q, unsigned max_q) {    
    min_in_q = min_q;
    max_in_q = max_q;
    max_buf = consum_buf_size;
    do_throttling = true;
    init();  
}

template <typename T>
t_queue<T>::t_queue (short unsigned n) {  /* Called with internal buffering */
    max_buf = n;
    do_throttling = false;
    feed_buf.reserve(n);
    consum_buf.reserve(n);
    init();
}

/* ---------------- close  -----------------------------------
 * Close the queue and tell anybody who might be waiting that
 * they should look for their last data.
 */
template <typename T>
void
t_queue<T>::close() {
    empty_closed |= CLOSED;
    std::unique_lock<std::mutex> refill_lock(refill_mtx);
    need_refill.notify_one(); /* say we are really finished */
}

/* ---------------- alive  -----------------------------------
 * This tells us if the queue is still alive. It does the
 * waiting if it is empty.
 * If the queue is empty and it is closed and the local buffer
 * is empty, than we are really fished and return false.
 */
template <typename T>
bool
t_queue<T>::alive () {
    const unsigned char e_c = empty_closed; /* We only need to access atomic var once */
    const unsigned char really_closed = (Q_EMPTY | CLOSED | C_BUF_EMPTY);

    if ( !(c_buf_was_empty) || (atm_size != 0)) /* most common case */
        return true;

    if ( (e_c & really_closed) == really_closed) 
        return false;

    /* Reaching here means the queue is not closed, but the buffer is empty */
    if (atm_size == 0) {
        if (do_throttling)
            if (throttled && (atm_size <= min_in_q))
                throttled_wait.notify_one();
        std::unique_lock<std::mutex> refill_lock(refill_mtx);
        alive_wants_notification = true;
        
/*      The next while condition is a guard against "spurious" wakeups. */
        while ((atm_size == 0) &&  ((empty_closed & really_closed) != really_closed))
            need_refill.wait(refill_lock);
    }
    
    /* We have waited, but it could be that the queue simply closed */
    if (atm_size == 0)
        return false;
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
            if (p_q.empty()) /* This has to be done while locked */
                to_change |= Q_EMPTY;
        }
        if (to_get) {
            empty_closed &= (~C_BUF_EMPTY);
            c_buf_was_empty = false;
            atomic_fetch_sub (&atm_size, to_get);
        }
        consum_cnt = 0;
    }

    tmp = consum_buf [ consum_cnt++];

    if (consum_cnt == consum_buf.size()) {
        to_change |= C_BUF_EMPTY;
        c_buf_was_empty = true;
    }
    if (to_change) /* This announces that we think the queue is empty */
        empty_closed |= to_change;      /* and/or our buffer is empty */

    if (do_throttling)
        if (throttled && (atm_size <= min_in_q))
            throttled_wait.notify_one();

    return tmp;
}

/* ---------------- push    ----------------------------------
 */
template <typename T>
void
t_queue<T>::push(const T t) {
    const unsigned char not_empty = ~Q_EMPTY;
    {
        std::lock_guard<std::mutex> lock (q_mtx);
        p_q.push (t);
    }
    empty_closed &= not_empty;
    atm_size ++;
    if (alive_wants_notification) {
/*      man pages suggest the next lock, but it does not seem necessary */
        std::unique_lock<std::mutex> refill_lock(refill_mtx);
        alive_wants_notification = false;
    }
    need_refill.notify_one();
    if (do_throttling) {
        if (atm_size >= max_in_q) {
            std::unique_lock<std::mutex> throttle_lock(throttle_mtx);
            throttled = true;
            std::cerr <<__func__ << " waiting";
            throttled_wait.wait (throttle_lock);
            throttled = false;
        }
    }
}
#endif /* T_QUEUE_TCC */
