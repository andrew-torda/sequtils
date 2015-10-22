/*
 * 18 oct 2015
 * Can only be included after <mutex> and <queue>
 */
#ifndef T_QUEUE_H
#define T_QUEUE_H
#ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wpadded"
#endif /* clang */

template <typename T>
class t_queue {
private:
    std::queue<T> p_q;
    std::mutex q_mtx;
    std::condition_variable need_refill;
    std::mutex refill_mtx;
    std::atomic<bool> alive_wants_notification;
    std::atomic<bool> closed;
    unsigned n;
public:
    t_queue () {
        alive_wants_notification = false;
        closed = false;
    }

    bool is_closed () {
        return closed;}

    void close() {
        closed = true;
        need_refill.notify_one();
    }

    bool empty() {
        std::lock_guard<std::mutex> lock (q_mtx);
        return p_q.empty();
    }

    bool alive () {
        if (closed)
            if (empty())
                return (false);
        if (empty()) {
            std::unique_lock<std::mutex> refill_lock(refill_mtx);
            alive_wants_notification = true;
            need_refill.wait(refill_lock);
            if (empty())  // Maybe it really was empty
                return false;
        }
        return true;
    }

    const T &front() {
        std::unique_lock<std::mutex> refill_lock(refill_mtx);
        return p_q.front();
        //        const T & tmp = p_q.front();
        //        q_mtx.unlock();
        //        return tmp;
    }

    void push(const T t) {
        {
            std::lock_guard<std::mutex> lock (q_mtx);
            p_q.push (t);
        }
        if (alive_wants_notification) {
            alive_wants_notification = false;
            need_refill.notify_one();
        }
    }

    size_t size() {
        return p_q.size(); }

    void pop () {
        std::lock_guard<std::mutex> lock (q_mtx);
        p_q.pop();
    }
};

#ifdef __clang__
#    pragma clang diagnostic pop
#endif /* clang */

#endif /* T_QUEUE_H */
