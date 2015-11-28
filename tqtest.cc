/* ---------------- testing ----------------------------------
 * This is for testing the queue.
 */

#include <iostream>
#include <thread>

#include <atomic>              /* These are necessary before */
#include <condition_variable>  /* including t_queue.hh */
#include <mutex>
#include <queue>

#include "t_queue.hh"

#include "delay.hh"

static void breaker() {}
struct s {
    unsigned  i;
    unsigned j;
};

static unsigned maxnum = 50000;
/* ---------------- consumer --------------------------------- */
static void
consumer (t_queue <struct s> &q)
{
    unsigned n = 0;
    while (q.alive()) {
        struct s s = q.front_and_pop();
        if (!(s.i % 5000) || s.i > (maxnum-4))
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

    {
        std::cout << "test with 3 ints\n";
        t_queue <struct s> q (10, 11, 20);
        std::thread cnsm (consumer, std::ref(q));
        std::thread f ( feeder, std::ref(q));

        f.join();
        cnsm.join();
        std::cout << '\n';
    }

    {
        std::cout << "test with one int.. ";
        t_queue <struct s> q (10);
        std::thread cnsm (consumer, std::ref(q));
        std::thread f ( feeder, std::ref(q));
        f.join();
        cnsm.join();
        std::cout << "OK\n";
    }
    {
        std::cout << "test with 2 ints.. ";
        t_queue <struct s> q (8, 1000);
        breaker();
        std::thread cnsm (consumer, std::ref(q));
        std::thread f ( feeder, std::ref(q));
        f.join();
        cnsm.join();
        std::cout << "OK\n";
    }

    {
        std::cout << "test with no ints.. ";
        t_queue <struct s> q;
        std::thread cnsm (consumer, std::ref(q));
        std::thread f ( feeder, std::ref(q));
        f.join();
        cnsm.join();
        std::cout << "OK\n";
    }
    
    return (EXIT_SUCCESS);
}

