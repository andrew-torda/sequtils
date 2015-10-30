/* 8 oct 2015
 * I have an MSA and want to
 *  - get rid of sequences which seem to have monster insertions
 *  - get rid of empty columns
 * Overall scheme
 *   Read the MSA, but only store the comments and put them in a hash.
 *   Read a list of magic sequences, that must be kept.
 *   Read the distance matrix and build a list.
 *   Go back to the MSA, copy entries to keep in to the output file.
 * TODO
 * - very important - when we are given a bad sequence, the whole thing crashes !
 *   shut the queue down on error.
 * - add a check when reading sequences for HHHHH. If one is found, print out a warning.
 * - add an option to remove all gap characters and rewrite the sequences
 */

#include <cerrno>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <random>
#include <queue>
#include <map>

#include <atomic>
#include <mutex>
#include <thread>

#include <vector>

//#include "delay.hh" /* Only during debugging */
#include "distmat_rd.hh"
#include "fseq.hh"
#include "mgetline.hh"
#include "t_queue.hh"
using namespace std;

/* ---------------- structures and constants ----------------- */
static const unsigned N_SEQBUF = 512;
static const unsigned SEQ_LINE_LEN = 60; /* How many chars per line output seqs */
static const char    *SEED_STR = "_seed_"; /* mafft marker for seed alignments */

static const unsigned char NOBODY = 0;
static const unsigned char S_1    = 1;
static const unsigned char S_2    = 2;

static const int DFLT_SEED = 180077;

static std::default_random_engine r_engine{};
static void breaker(){}

/* ---------------- usage ------------------------------------ */
static int usage ( const char *progname, const char *s)
{
    static const char *u
        = ": [-sv -a sacred_file -c choice -e seed] mult_seq_align.msa \
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
    unsigned i = 0;
        while (q_fs.alive()) {
        v_fs v = q_fs.front();
        q_fs.pop();
        for (v_fs::iterator it = v.begin() ; it != v.end(); it++) {
            fseq fs = *it;
            fseq_prop f_p (*it);
            f_map [fs.get_cmmt()] = f_p;
        }
    }

#   ifdef use_one_seq
    while (q_fs.alive()) {
        fseq fs = q_fs.front();
//      cerr<< __func__<< " got "<< fs.get_cmmt().substr(0,10) << "\n";
        q_fs.pop();
        fseq_prop f_p (fs);
        f_map[fs.get_cmmt()] = f_p;
    }
#   endif /* use_one_seq */
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
    { /* look at the first sequence, get the length, so we can check on */
        streampos pos = infile.tellg();             /* subsequent reads */
        fseq fs(infile, 0);
        len = fs.get_seq().size();
        infile.seekg (pos);
    }

    t_queue <v_fs> q_fs;
    thread t1 (from_queue, ref(q_fs), ref(f_map));

    {
        fseq fs;
        unsigned n = 0;
        unsigned scount = 0;
        v_fs tmp_v_fs;
        while (fs.fill (infile, len)) {
            scount++;                      /* Put packages of N_SEQBUF */
            tmp_v_fs.push_back (fs);      /* into the queue, rather than */
            if (++n == N_SEQBUF) {         /* sequences one at a */
                q_fs.push (tmp_v_fs);
                n = 0;
            }
        }
        if (tmp_v_fs.size())                              /* catch the leftovers */
            q_fs.push (tmp_v_fs);
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
        cerr << __func__ << ": opening " << sacred_fname
             << ": " << strerror(errno) << "\n";
        *sacred_ret = EXIT_FAILURE;
        return;
    }
    while (mgetline (sac_file, s))
        v_sacred.push_back (s);
    sac_file.close();
    v_sacred.shrink_to_fit();
    *sacred_ret = EXIT_SUCCESS;
}

/* ---------------- check_lists ------------------------------
 * Make sure that the sequences given in the distance file
 * exist in the alignment file, as store in f_map.
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
    return ret;
}

/* ---------------- decider functions ------------------------
 * We have several possibilities for deciding which member of a
 * pair to delete. We use one of these, as pointed to by a
 * function pointer.
 */
typedef unsigned char decider_f (const fseq_prop&, const fseq_prop&);

static unsigned char
always_first (const fseq_prop &f1, const fseq_prop &f2)
{
    return S_1;
}

static unsigned char
always_second (const fseq_prop &f1, const fseq_prop &f2)
{
    return S_2;
}
static unsigned char
more_gaps (const fseq_prop &f1, const fseq_prop &f2)
{
    cerr << "f1 and f2 gaps, " << f1.ngap << " "<< f2.ngap<<"\n";
    if (f1.ngap >= f2.ngap)
        return S_1;
    return S_2;
}

static unsigned char
decide_random(const fseq_prop &f1, const fseq_prop &f2)
{
    uniform_int_distribution<int> d{0,1};
    int r = d(r_engine);
    if (r == 1)
        return S_1;
    return S_2;
}

static unsigned char
decide_longer(const fseq_prop &f1, const fseq_prop &f2)
{
    if (f1.ngap < f2.ngap)
        return S_2;
    return S_1;
}

static decider_f *
set_up_choice (const string &s)
{
    map <string, decider_f*> choice_map;
    choice_map["first"]  = always_first;  /* Table of keywords and */
    choice_map["second"] = always_second; /* function pointers */
    choice_map["random"] = decide_random;
    choice_map["longer"] = decide_longer;

    const map<string, decider_f *>::iterator missing = choice_map.end();
    const map<string, decider_f *>::iterator ent = choice_map.find(s);
    if (ent == missing) {
        cerr << "random choice type " << s << " not known. Try one of \n";
        map<string, decider_f *>::iterator it = choice_map.begin();
        for (; it != missing; it++)
            cout << it->first << " ";
        cout << endl;
        return nullptr;
    }
    
    return ent->second;
}

/* ---------------- choose_seq   -----------------------------
 * Given two seq props, choose one for deletion. Return 1 or 2
 * or 0 for nobody.
 * We will have to expand this to consider different options.
 */
static unsigned char
choose_seq (const fseq_prop &f1, const fseq_prop &f2, decider_f *choice)
{
    if (f1.is_sacred() && f2.is_sacred())
        return NOBODY;
    if (f1.is_sacred())
        return S_2;
    else if(f2.is_sacred())
        return S_1;
    return (choice (f1, f2));
}

/* ---------------- remove_seq -------------------------------
 * Walk down the list of distances, deciding who to delete.
 */
static void
remove_seq (map<string, fseq_prop> &f_map, vector<string> &v_cmt,
            vector<dist_entry> &v_dist, const unsigned to_keep,
            decider_f *choice)
{
    vector<dist_entry>::iterator it = v_dist.begin();
    unsigned kept = f_map.size();
    for (; f_map.size() > to_keep; it++) {
        if (it == v_dist.end())
            break;
        string &s1 = v_cmt[it->ndx1];
        string &s2 = v_cmt[it->ndx2];
        const map<string, fseq_prop>::iterator missing = f_map.end();
        const map<string, fseq_prop>::iterator f1 = f_map.find(s1);
        const map<string, fseq_prop>::iterator f2 = f_map.find(s2);
        if ((f1 == missing) || (f2 == missing))
            continue;
        const unsigned char c = choose_seq(f1->second, f2->second, choice);
        switch (c) {
        case NOBODY:
            continue;           break;
        case S_1:
            f_map.erase(s1);    break;
        case S_2:
            f_map.erase(s2);    break;
        }
    }
}

/* ---------------- remove_seeds -----------------------------
 */
static void
remove_seeds (map<string, fseq_prop> &f_map, vector<string> &v_cmt)
{
    vector<string>::iterator it = v_cmt.begin();
    for (; it != v_cmt.end(); it++)
        if (it->find (SEED_STR) != string::npos)
            f_map.erase (*it);
}

/* ---------------- write_kept_seq ---------------------------
 * This is the final writing of sequences that we want to keep.
 */
static int
write_kept_seq (const char *in_fname, const char *out_fname,
                map<string, fseq_prop> &f_map)
{
    ifstream in_file (in_fname);
    fseq fs;
    if (! in_file) {
        cerr << __func__ << "open fail on " << in_fname << "\n";
        return EXIT_FAILURE;
    }
    ofstream out_file (out_fname);
    if ( ! out_file) {
        cerr << __func__ << ": opening " << out_fname  << ": " << strerror(errno) << "\n";
        return EXIT_FAILURE;
    }

    while (fs.fill (in_file, 0)) {
        const map<string, fseq_prop>::iterator missing = f_map.end();
        const map<string, fseq_prop>::iterator f1 = f_map.find(fs.get_cmmt());
        if (f1 != missing) {
            out_file << fs.get_cmmt() << std::endl;
            unsigned done = 0, to_go;
            string s = fs.get_seq();
            to_go = s.length();
            while (to_go) {
                size_t this_line = SEQ_LINE_LEN;
                if (SEQ_LINE_LEN > to_go)
                    this_line = to_go;
                out_file << s.substr (done, this_line)<< std::endl;
                done += this_line;
                to_go -= this_line;
            }
        }

    }

    in_file.close();
    out_file.close();
    return (EXIT_SUCCESS);
}

static void
our_terminate (void) {
    breaker();
    exit (EXIT_FAILURE);
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
    string choice_name, seed_str;
    int seed;
 
    /*    set_terminate(our_terminate); */
    while ((c = getopt(argc, argv, "a:c:e:sv")) != -1) {
        switch (c) {
        case 'a':
            sacred_fname = optarg;                                     break;
        case 'c':
            choice_name += optarg;                                     break;
        case 'e':
            seed_str = optarg;                                         break;
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

    if ((argc - optind) < 4)
        usage (progname, " too few arguments");
    const char *in_fname     = argv[optind++];
    const char *dist_fname   = argv[optind++];
    const char *out_fname    = argv[optind++];
    const char *to_keep_str  = argv[optind++];
    const unsigned n_to_keep = stoul (to_keep_str);
    if (seed_str.length())
        seed = stoi (seed_str);
    else
        seed = DFLT_SEED;
    r_engine.seed( seed);
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

    decider_f *choice = always_first;

    if (choice_name.size())
        if ((choice = set_up_choice(choice_name)) == nullptr) {
            gsl_thr.join(); return EXIT_FAILURE;}


    vector<dist_entry> v_dist;

    vector<string> v_cmt;

    try {
        read_distmat (dist_fname, v_dist, v_cmt) ;
    } catch (runtime_error &e) {
        gsl_thr.join(); sac_thr.join();
        cerr << e.what();  return EXIT_FAILURE; }

    gsl_thr.join();
    if (gsl_ret != EXIT_SUCCESS) {
        cerr << __func__ << " error in get_seq_list\n";
        return EXIT_FAILURE;
    }

    if (check_lists (f_map, v_cmt) == EXIT_FAILURE)
        return EXIT_FAILURE;
    if (seedflag)
        remove_seeds (f_map, v_cmt);

    if (sacred_fname) {
        sac_thr.join();
        if (sacred_ret != EXIT_SUCCESS) {
            cerr << __func__ << " err reading sacred file from "<< sacred_fname;
            return EXIT_FAILURE;
        }
        if (mark_sacred (f_map, v_sacred) != EXIT_SUCCESS)
            return EXIT_FAILURE;
    }

    remove_seq (f_map, v_cmt, v_dist, n_to_keep, choice);
    if (write_kept_seq (in_fname, out_fname, f_map) != EXIT_SUCCESS)
        return EXIT_FAILURE;

#   undef check_the_map_is_ok
#   ifdef check_the_map_is_ok
        cout << "dump f_map:\n";
        for (map<string,fseq_prop>::iterator it=f_map.begin(); it!=f_map.end(); it++)
            cout << it->first.substr (0,20) << "\n";
        cout << "f_map size: "<< f_map.size() << "\n";
#   endif

    return EXIT_SUCCESS;
}
