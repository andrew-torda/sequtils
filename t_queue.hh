/*
 * 18 oct 2015
 * Can only be included after <mutex> and <queue>
 * Limited to one writer and reader.
 */
#ifndef T_QUEUE_H
#define T_QUEUE_H
#ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wpadded"
#endif /* clang */

#include <condition_variable>
/* #include <iomanip> */  /* only for debugging output */
#include "delay.hh"
template <typename T>
class t_queue {
private:
    std::queue<T> p_q;
    std::mutex q_mtx;
    std::mutex refill_mtx;
    std::mutex throttle_mtx;
    std::condition_variable need_refill;
    std::condition_variable throttled_wait;
    std::atomic<size_t> atm_size;
    std::atomic<bool>   alive_wants_notification;
    std::atomic<bool>   closed;
    std::atomic<bool>   throttled;
    unsigned min_in_q, max_in_q;
    //  short unsigned n_close, n_wait, n_tot;

    bool do_throttling; /* Can the queue be throttled ? */

    bool
    empty() {
        if (atm_size == 0)
            return true;
        return false;
    }

    void init() {
        alive_wants_notification = false;
        closed = false;
        throttled = false;
        atm_size = 0;
    };
public:
    t_queue () {
        do_throttling = false;
        init();
    }
    t_queue (unsigned min_q, unsigned max_q) {
        min_in_q = min_q;
        max_in_q = max_q;
        do_throttling = true;
        init();
    }

    bool
    is_closed () {
        return closed;}

    void
    close() {
        closed = true;  /* It is possible that the reader was waiting. */
        std::unique_lock<std::mutex> refill_lock(refill_mtx);
        need_refill.notify_one(); /* say we are really finished */
    }

    bool
    alive () {
        if (empty()) {
            if (closed) {
//              std::cerr << __func__ << " n_tot n_wait n_close" << std::setw (6)<< n_tot << std::setw(6) << n_wait << std::setw(6) << ++n_close << "\n";
                return false;
            }
            if (do_throttling)
                if (throttled && (atm_size <= min_in_q))
                    throttled_wait.notify_one();
            std::unique_lock<std::mutex> refill_lock(refill_mtx);
            alive_wants_notification = true;
            need_refill.wait(refill_lock, [this](){return atm_size != 0; });
            alive_wants_notification = false;
//          n_wait++;
        }
//      n_tot++;
        return true;
    }

/*  If I combine front() and pop(), I only have to lock the queue once. */
/*  Unfortunately, I cannot get it to work if I use a reference to the */
/*  returned T. This is not tragic, but means I am copying unnecessarily. */
    const T
    front_and_pop() {
        T tmp;
        {
            std::lock_guard<std::mutex> lock (q_mtx);
            tmp = p_q.front();
            p_q.pop();
            atm_size = p_q.size();
        }
        if (do_throttling)
            if (throttled && (atm_size <= min_in_q))
                throttled_wait.notify_one();
        return tmp;
    }

    const T &
    front() {
        std::lock_guard<std::mutex> lock (q_mtx);
        return p_q.front();
    }
    
    void
    push(const T t) {
        {
            std::lock_guard<std::mutex> lock (q_mtx);
            p_q.push (t);
            atm_size = p_q.size();
        }
        if (alive_wants_notification) {
/*          man pages suggest the next lock, but it does not seem necessary */
/*          std::unique_lock<std::mutex> refill_lock(refill_mtx); */
            need_refill.notify_one();
        }
        if (do_throttling) {
            if (atm_size >= max_in_q) {
                std::unique_lock<std::mutex> throttle_lock(throttle_mtx);
                throttled = true;
                throttled_wait.wait (throttle_lock);
                throttled = false;
            }
        }
    }

    void
    pop () {
        std::lock_guard<std::mutex> lock (q_mtx);
        p_q.pop();
        atm_size = p_q.size();
    }
};

#ifdef __clang__
#    pragma clang diagnostic pop
#endif /* clang */

#endif /* T_QUEUE_H */
