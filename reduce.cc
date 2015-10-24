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

#include <algorithm> /* only for playing with sort. Not really needed */
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

#include <vector>

#include "distmat_rd.hh"

#include "fseq.hh"
#include "t_queue.hh"
using namespace std;

/* ---------------- structures and constants ----------------- */
static const unsigned N_SEQBUF = 512;

/* ---------------- usage ------------------------------------ */
static int usage ( const char *progname, const char *s)
{
    static const char *u
        = ": [-sv -a sacred_file] mult_seq_align.msa \
dist_mat.hat2 outfile.msa n_to_keep\n";
    cerr << progname << s << "\n";
    cerr << progname << u;
    return EXIT_FAILURE;
}

typedef vector<fseq> v_fs;
/* ---------------- from_queue -------------------------------
 * We have bundles of sequences in a vector. Pull each from
 * the queue
 */
static void
from_queue (t_queue <v_fs> &q_fs, map<string, fseq_prop> &f_map)  {
    while (q_fs.alive()) {
        v_fs v = q_fs.front();
        q_fs.pop();
        for (v_fs::iterator it = v.begin() ; it != v.end(); it++) {
            fseq fs = *it;
            fseq_prop f_p (*it);
            f_map [fs.get_cmmt()] = f_p;
        }
    }
}

/* ---------------- get_seq_list -----------------------------
 * From a multiple sequence alignment, read the sequences and
 * just fill out the information.
 * If we are going to put this in a thread, we have to catch exceptions
 * here
 */
static void
get_seq_list (map<string, fseq_prop> &f_map, const char *in_fname, int *ret) {
    string errmsg = __func__;
    size_t len;
    ifstream infile (in_fname);
    if (!infile) {
        errmsg += ": opening " + string (in_fname) + ": " + strerror(errno) + "\n";
        cerr << errmsg;
        *ret = EXIT_FAILURE;
        return;
    }
        
    {
        streampos pos = infile.tellg();
        fseq fs(infile, 0);     /* get first sequence */
        len = fs.get_seq().size();
        infile.seekg (pos);
    }

    t_queue <v_fs> q_fs;
    thread t1 (from_queue, ref(q_fs), ref(f_map)); // without ref(), it does not compile
    {
        fseq fs;
        unsigned n = 0;
        unsigned scount = 0;
        v_fs v_fs;
        while (fs.fill (infile, len)) {
            scount++;
            v_fs.push_back (fs);
            if (++n == N_SEQBUF) {
                q_fs.push (v_fs);
                v_fs.clear();
                n = 0;
            }
        }
        if (v_fs.size())                              /* catch the leftovers */
            q_fs.push (v_fs);
        cout << "read "<< scount << " seqs\n";
    }

    infile.close(); /* Stroustrup would just wait until it went out of scope */
    q_fs.close();

    t1.join();
    *ret = EXIT_SUCCESS;
}

/* ---------------- get_sacred -------------------------------
 * read the file of sacred sequences.
 * Could be called as a thread, so no exceptions allowed and
 * put success/failure in the last argument.
 */
static void
get_sacred (const char *sacred_fname, vector<string> &v_sacred, int *sacred_ret)
{
    ifstream sac_file (sacred_fname);
    string s;
    if (!sac_file) {
        cerr << __func__ << ": opening " << string (sacred_fname)
             << ": " << strerror(errno) << "\n";
        *sacred_ret = EXIT_FAILURE;
        return;
    }
    while (getline (sac_file, s))
        v_sacred.push_back (s);
    *sacred_ret = EXIT_SUCCESS;
    sac_file.close();
    v_sacred.shrink_to_fit();
    return;
}
static void breaker () {}
/* ---------------- check_lists ------------------------------
 */
static const int
check_lists ( const map<string, fseq_prop> &f_map, vector<string> &v_cmt)
{
    for (vector<string>::iterator it = v_cmt.begin(); it != v_cmt.end(); it++) {
        if (f_map.find(*it) == f_map.end()) {
            cerr << "Sequence " << *it << " in distmat file not found\n";
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

/* ---------------- mark_sacred ------------------------------
 * Take each sequence that has been marked as sacred.
 * Look for it in the f_map. If we find it, mark it as sacred.
 */
static int
mark_sacred (map<string, fseq_prop> &f_map, vector<string> &v_sacred)
{
    vector<string>::iterator it = v_sacred.begin() ;
    int ret = EXIT_SUCCESS;
    for ( ;it != v_sacred.end(); ++it) {
        if (f_map.find(*it) == f_map.end()) {
            cerr << __func__ << "seq starting " << *it << " not found\n";
            return EXIT_FAILURE;
        }
        f_map[*it].make_sacred();
    }
    return EXIT_SUCCESS;
}

/* ---------------- main  ------------------------------------ */
int
main (int argc, char *argv[])
{
    int c;
    short unsigned verbosity = 0;
    bool seedflag = false;
    const char *progname = argv[0];
    const char *sacred_fname = nullptr;
    bool eflag = false;
    while ((c = getopt(argc, argv, "a:sv")) != -1) {
        switch (c) {
        case 'a':
            sacred_fname = optarg;                                     break;     
        case 's':
            seedflag = true;                                           break;
        case 'v':
            verbosity++;                                               break;
        case ':':
            cerr << argv[0] << "Missing opt argument\n"; eflag = true; break;
        case '?':
            cerr << argv[0] << "Unknown option\n";       eflag = true; break;
        }
    }
    if (eflag)
        return (usage(progname, ""));

    optind++;
    if ((argc - optind) < 5)
        usage (progname, " too few arguments");
    const char *in_fname    = argv[++optind];
    const char *dist_fname  = argv[++optind];
    const char *out_fname   = argv[++optind];
    const char *to_keep_str = argv[++optind];
    const unsigned to_keep  = stoul (to_keep_str);

    cout << progname << ": using " << in_fname << " as MSA. "
         << dist_fname << " as distance matrix. Writing to " << out_fname << "\n";

    map<string, fseq_prop> f_map;
    int gsl_ret;
    thread gsl_thr (get_seq_list, ref(f_map), in_fname, &gsl_ret);

    vector<string> v_sacred;
    int sacred_ret;
    thread sac_thr;
    if (sacred_fname)
        sac_thr = thread (get_sacred, sacred_fname, ref(v_sacred), &sacred_ret);

    vector<dist_entry> v_dist;
    {
        vector<string> v_cmt;
        try {
            read_distmat (dist_fname, v_dist, v_cmt) ;
        } catch (runtime_error &e) {
            cerr << e.what();  return EXIT_FAILURE; }

        gsl_thr.join();
        if (gsl_ret != EXIT_SUCCESS) {
            cerr << __func__ << " error in get_seq_list\n";
            return EXIT_FAILURE;
        }
        if (check_lists (f_map, v_cmt) == EXIT_FAILURE)
            return EXIT_FAILURE;
    } // v_cmt goes out of scope and is cleared
    
    if (sacred_fname) {
        sac_thr.join();
        if (sacred_ret != EXIT_SUCCESS) {
            cerr << __func__ << " err reading sacred file from "<< sacred_fname;
            return EXIT_FAILURE;
        }
    }
    if (mark_sacred (f_map, v_sacred) != EXIT_SUCCESS)
        return EXIT_FAILURE;
    v_sacred.clear();
    cout << "now compare the lists from the two files\n";


    
#   undef check_the_map_is_ok
#   define check_the_map_is_ok
#   ifdef check_the_map_is_ok
        cout << "dump f_map:\n";
        for (map<string,fseq_prop>::iterator it=f_map.begin(); it!=f_map.end(); it++)
            cout << it->first.substr (0,20) << "\n";
        cout << "f_map size: "<< f_map.size() << "\n";
#   endif

        
    return EXIT_SUCCESS;
}
