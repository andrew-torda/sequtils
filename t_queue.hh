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

#include <atomic>  /* These three are for threading */
#include <condition_variable>
#include <mutex>
#include <thread>

/* ---------------- t_queue  --------------------------------- */
template <typename T>
class t_queue {
private:
    std::queue<T> p_q;
    std::mutex q_mtx;
    std::mutex refill_mtx;
    std::mutex throttle_mtx;
    std::condition_variable need_refill;
    std::condition_variable throttled_wait;
    std::atomic<typename std::queue<T>::size_type> atm_size;
    std::atomic<bool>   throttled;
    std::atomic<unsigned char> q_state;


    std::vector<T> feed_buf;
    std::vector<T> consum_buf;
    unsigned min_in_q, max_in_q;
    unsigned max_buf;
    unsigned feed_cnt, consum_cnt;
    bool c_buf_was_empty;
    bool feed_buf_full;

    bool do_throttling; /* Can the queue be throttled ? */
    void init();
    void flush();
public:
    t_queue ();
    t_queue (short unsigned n);
    t_queue (unsigned min_q, unsigned max_q);
    t_queue (short unsigned prod_buf_siz, unsigned min_q, unsigned max_q);
    void close();
    bool alive();
    const T front_and_pop();
    void push(const T t);
};

#ifdef __clang__
#    pragma clang diagnostic pop
#endif /* clang */
#include "t_queue.tcc"
#endif /* T_QUEUE_H */
