/* ---------------- testing ----------------------------------
 * This is for testing the queue.
 */

#include <cstdlib>

#include <iostream>
#include <thread>

#include <atomic>              /* These are necessary before */
#include <condition_variable>  /* including t_queue.hh */
#include <mutex>
#include <queue>

#include "t_queue.hh"


static void breaker() {}
struct s {
    unsigned  i;
    unsigned j;
};

static unsigned maxnum = 150000;
/* ---------------- consumer --------------------------------- */
static void
consumer (t_queue <struct s> &q)
{
    unsigned n = 0;
    while (q.alive()) {
        struct s s = q.front_and_pop();
        if (s.i < 10 || !(s.i % 50000) || s.i > (maxnum-4))
            std::cerr << "cnsm " << " "<< s.i << " ";
        n++;
    }
    std::cout << "\n"<<__func__ << " " << n << " items\n";
}

/* ---------------- feeder ----------------------------------- */
static void
feeder (t_queue <struct s> &q)
{
    unsigned n = 0;
    for (unsigned i = 0; i < maxnum; i++) {
        struct s s;
        s.i = i;
        s.j = 7;
        q.push(s);
        n++;
    }
    q.close();
    std::cout << "\n"<<__func__ << " queued "<<n<<  " items\n";
}


/* ---------------- main  ------------------------------------ */
int
main ()
{
    t_queue <struct s> q (10, 10, 10, 10);
    std::thread cnsm (consumer, std::ref(q));
    std::thread f ( feeder, std::ref(q));
    f.join();
    cnsm.join();
    return (EXIT_SUCCESS);
}

