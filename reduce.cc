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
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>

#include <csignal>
#include <stdexcept>
#include <queue>
#include <map>
#include <mutex>
#include <thread>

#include "fseq.hh"
#include "t_queue.hh"
using namespace std;
/* ---------------- structures and constants ----------------- */
//typedef class t_queue<class pair_info> pair_queue;

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
static void
from_queue (t_queue <pair_info> &q_f_in, map<string, fseq_prop> &f_map)  {
        
    while (q_f_in.begin() != q_f_in.end()) {
        pair_info p_i = q_f_in.front();
        cout<< "qsize is " << q_f_in.size() << " cmt is "<< p_i.fseq.get_cmmt() << "\n";
        q_f_in.pop_front();
        fseq_prop f_p (p_i.fseq);
        cout << "f_p with "<< f_p.ngap << "gaps \n";
        f_map [p_i.fseq.get_cmmt()] = f_p;        
    }
}


#ifdef want_bg
/* -------------------------------------------------------------------------------- */
static void bg (int i, t_queue <pair_info> q_p_i) {
    cout << "bg hello sizeof " << q_p_i.size() << " and i is "<<i<<"\n";
    return;
}
#endif /* want_bg */

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
    t_queue <pair_info> q_p_i;
    //    auto f1 = bind (from_queue, q_p_i, f_map);
    //    thread t1 (bind (from_queue, q_p_i, f_map));
    //    thread t1 (from_queue, q_p_i, f_map);
    {
        fseq fseq (infile, len);  // destructor is called here. why ?
        do {
            fseq_prop fseq_prop(fseq);
            pair_info pair_info (fseq_prop, fseq);
            q_p_i.push_back (pair_info);
        } while (fseq.replace (infile, len));
    }
    //    auto f1 = std::bind (bg, 2, q_p_i);
    //    thread t1 = thread (f1);
    infile.close(); /* Stroustrup would just wait until it went out of scope */
    q_p_i.close();

    from_queue (q_p_i, f_map);  // this is what will go into a thread later

    //    t1.join();
    return EXIT_SUCCESS;

}

/* ---------------- main  ------------------------------------ */
int
main (int argc, char *argv[])
{
    int c;
    const char *progname = argv[0];
    cout << "argc is "<< argc << "\n";
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
#   define check_the_map_is_ok 1
#   ifdef check_the_map_is_ok
    cout << "f-map has " << f_map.size() << " elem\n";
    for (map<string,fseq_prop>::iterator it=f_map.begin(); it!=f_map.end(); ++it)
        cout << it->first << " " << it->second.is_sacred() << "\n";
#endif


    return EXIT_SUCCESS;
}
