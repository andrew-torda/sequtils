/*
 * 18 oct 2015
 * Can only be included after <mutex> and <queue>
 */
#ifndef T_QUEUE_H
#define T_QUEUE_H
#include <cstddef>
class t_queue {
private:
    std::deque<pair_info> p_q;
    std::mutex q_mtx;
    pair_info no_pair;
    bool closed;  /* This must be set when we have no more data to add */
public:
    t_queue () {closed = false;}
    const bool is_closed () {
        return closed;}
    bool close() {
        closed = true;}
    const pair_info &front () {
        if (! p_q.size()) {
            if (closed) {
                return no_pair;
            } else {
                std::cout << __func__ << " waiting  ";
                // set variable waiting
                // wait
            }
        }
        return p_q.front();
    }
    size_t size() {
        return p_q.size(); }
    void pop () {
        return p_q.pop_front(); }
    void push_back(pair_info p_i) {
        p_q.push_back (p_i); }
};

#endif /* T_QUEUE_H */
