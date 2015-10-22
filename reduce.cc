/* 8 oct 2015
 * I have an MSA and want to
 *  - get rid of sequences which seem to have monster insertions
 *  - get rid of empty columns
 * Overall scheme
 *   Read the MSA, but only store the comments and put them in a hash.
 *   Read a list of magic sequences, that must be kept.
 *   Read the distance matrix and build a list.
 *   Go back to the MSA, copy entries to keep in to the output file.
 * Options
 *   -s discard seeds. 
 */

#include <cerrno>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>

#include <csignal>
#include <stdexcept>
#include <queue>
#include <map>

#include <atomic>
#include <mutex>
#include <thread>

#include "fseq.hh"
#include "t_queue.hh"
using namespace std;

/* ---------------- structures and constants ----------------- */
struct pair_info {
    class fseq_prop fseq_prop;
    class fseq fs;
    //#   error make a constructor which puts a lock here
};

/* ---------------- usage ------------------------------------ */

static int usage ( const char *progname, const char *s)
{
    static const char *u = " infile outfile\n";
    cerr << progname << s << "\n";
    cerr << progname << u;
    exit (EXIT_FAILURE);
    return EXIT_FAILURE;
}


/* ---------------- from_queue -------------------------------
 * better tactics are to use an iterator to read from the queue
 * and use my own end() operator.
 */
static int
from_queue (t_queue <pair_info> &q_f_in, map<string, fseq_prop> &f_map)  {
    unsigned n = 0;
    while (q_f_in.alive()) {
        //        pair_info p_i = q_f_in.front();  //is this reading too far ?
//      cerr << __func__ << ": n is " << n++ << " qsize is "
//      << q_f_in.size() << " cmt is "<< p_i.fseq.get_cmmt() << "\n";
        fseq fs = q
        q_f_in.pop();
        fseq_prop f_p (p_i.fs);

        f_map [p_i.fs.get_cmmt()] = f_p;
    }
    return 0;
}

#include <chrono>

/* ---------------- get_seq_list -----------------------------
 * From a multiple sequence alignment, read the sequences and
 * just fill out the information.
 */
static int
get_seq_list (map<string, fseq_prop> &f_map, const char *in_fname) {
    string errmsg = __func__;
    size_t len;
    ifstream infile (in_fname);
    if (!infile) {
        errmsg += ": opening " + string (in_fname) + ": " + strerror(errno) + "\n";
        throw runtime_error (errmsg);
    }
        
    {
        streampos pos = infile.tellg();
        fseq fseq(infile, 0);     /* get first sequence */
        len = fseq_prop(fseq).length;
        infile.seekg (pos);
    }

//    t_queue <pair_info> q_p_i;
    t_queue <fseq> q_fs;
    thread t1 (from_queue, ref(q_p_i), ref(f_map)); // without ref(), it does not compile
    {
        fseq fs;
        int n = 0;
        while (fs.fill (infile, len)) {
            //            fseq_prop fsp;
            //            const pair_info p_i = {fsp, fs};
            q_p_i.push (fs);
        }
#       ifdef old_and_scraggly
        this_thread::sleep_for(std::chrono::milliseconds(1));;
        cout << __func__ << ": filling q, n is "<< n++ << "\n";
#       endif /* old_and_scraggly */
    }

    infile.close(); /* Stroustrup would just wait until it went out of scope */
    q_p_i.close();

    from_queue (q_p_i, f_map);  // this is what will go into a thread later
    t1.join();
    return EXIT_SUCCESS;

}

/* ---------------- main  ------------------------------------ */
int
main (int argc, char *argv[])
{
    int c;
    const char *progname = argv[0];
    bool sflag = false;
    
    while ((c = getopt(argc, argv, "s")) != -1) {
        switch (c) {
        case 's':
            sflag = true;                                        break;
        case '?':
            usage(progname, "");                                 break;
        }
    }
    if ((argc - optind) < 2)
        usage (progname, " too few arguments");
    const char *in_fname = argv[optind++];
    const char *out_fname = argv[optind];
    cout << "from " << in_fname << " to " << out_fname << "\n";

    map<string, fseq_prop> f_map;

    try {
        get_seq_list (f_map, in_fname);
    } catch (runtime_error &e) {
        cerr << e.what();
        return EXIT_FAILURE;
    }

#   undef check_the_map_is_ok
#   ifdef check_the_map_is_ok
    cout << "dump f_map size: " << f_map.size() << " elem\n";
    for (map<string,fseq_prop>::iterator it=f_map.begin(); it!=f_map.end(); ++it)
        cout << it->first << " " << it->second.is_sacred() << "\n";
#endif


    return EXIT_SUCCESS;
}
