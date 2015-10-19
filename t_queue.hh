/*
 * 18 oct 2015
 * Can only be included after <mutex> and <queue>
 */
#ifndef T_QUEUE_H
#define T_QUEUE_H

class t_queue {
private:
    std::deque<pair_info> p_q;
    std::mutex q_mtx;
    pair_info no_pair;
    bool closed;  /* This must be set when we have no more data to add */
public:
    t_queue () {closed = false;}
    bool is_closed () {
        return closed;}
    void close() {
        closed = true;}
    const pair_info &front () {
        return p_q.front();}
    std::deque<pair_info>::iterator begin() {
        return p_q.begin(); }
    std::deque<pair_info>::iterator end() {
        if (closed || p_q.size())
            ; /* do nothing, reorder this stuff */
        else {
            std::cout << __func__ << " waiting for end";
            // set variable waiting
            //wait
        }
        return p_q.end();
    }
    size_t size() {
        return p_q.size(); }
    void pop () {
        return p_q.pop_front(); }
    void push_back(pair_info p_i) {
        p_q.push_back (p_i); }
};

#endif /* T_QUEUE_H */
