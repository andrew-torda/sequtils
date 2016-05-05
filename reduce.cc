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
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <random>
#include <queue>
#include <map>
#include <vector>

#include "bust.hh"
#include "distmat_rd.hh"
#include "fseq.hh"
#include "fseq_prop.hh"
#include "mgetline.hh"
#include "t_queue.hh"

using namespace std;

/* ---------------- structures and constants ----------------- */
static const char GAPCHAR = '-';
static const unsigned N_SEQBUF = 500;
static const unsigned SEQ_LINE_LEN = 60; /* How many chars per line output seqs */
static const char    *SEED_STR = "_seed_"; /* mafft marker for seed alignments */

static const unsigned char NOBODY = 0;
static const unsigned char S_1    = 1;
static const unsigned char S_2    = 2;

static const int DFLT_SEED = 180077;

struct seq_props {
    map<string, fseq_prop> f_map;
    size_t len;
};

/* ---------------- usage ------------------------------------ */
static int usage ( const char *progname, const char *s)
{
    static const char *u
        = ": [-fsv -a sacred_file -c choice -e seed] mult_seq_align.msa \
dist_mat.hat2 outfile.msa n_to_keep";
    return (bust(progname, s, "\n", progname, u, 0));
}

/* ---------------- from_queue -------------------------------
 * We have bundles of sequences in a vector. Pull each from
 * the queue
 */
static void
from_queue (t_queue <fseq> &q_fs, map<string, fseq_prop> &f_map)  {
    unsigned n = 0;
    while (q_fs.alive()) {
        fseq fs = q_fs.front_and_pop();
        fseq_prop f_p (fs);
        f_map [fs.get_cmmt()] = f_p;
        n++;
    }
    cout << __func__ << " read "<< n<< " seqs\n";
}

/* ---------------- get_seq_list -----------------------------
 * From a multiple sequence alignment, read the sequences and
 * just fill out the information.
 * If we are going to put this in a thread, we have to catch exceptions
 * here
 */
static void
get_seq_list (struct seq_props & s_props, const char *in_fname, int *ret) {
    string errmsg = __func__;
    size_t len;
    ifstream infile (in_fname);
    if (!infile) {
        *ret = EXIT_FAILURE;
        return (bust_void(__func__, "opening", in_fname, strerror(errno), 0));
    }
    { /* look at the first sequence, get the length, so we can check on */
        streampos pos = infile.tellg();             /* subsequent reads */
        fseq fs(infile, 0);
        len = fs.get_seq().size();
        infile.seekg (pos);
    }
    s_props.len = len;
    t_queue <fseq> q_fs(N_SEQBUF);
    thread t1 (from_queue, ref(q_fs), ref(s_props.f_map));

    {
        fseq fs;
        unsigned scount = 0;
        for (; fs.fill(infile, len); scount++)
            q_fs.push (fs);
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
        *sacred_ret = EXIT_FAILURE;
        return(bust_void(__func__, "opening", sacred_fname, ":", strerror(errno), 0));
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
static int
check_lists ( const map<string, fseq_prop> &f_map, vector<string> &v_cmt)
{
    unsigned n = 0;
    const char *s1 = "\" in distmat file not found\nIt was sequence number ";
    for (vector<string>::const_iterator it = v_cmt.begin(); it != v_cmt.end(); it++)
        if (f_map.find(*it) == f_map.end())
            return(bust(__func__, "Sequence \"", it->c_str(), s1, to_string(n).c_str(), 0));
    return EXIT_SUCCESS;
}

/* ---------------- mark_sacred ------------------------------
 * Take each sequence that has been marked as sacred.
 * Look for it in the f_map. If we find it, mark it as sacred.
 */
static int
mark_sacred (map<string, fseq_prop> &f_map, vector<string> &v_sacred)
{
    vector<string>::const_iterator it = v_sacred.begin() ;
    int ret = EXIT_SUCCESS;
    for ( ;it != v_sacred.end(); ++it) {
        if (f_map.find(*it) == f_map.end())
            return (bust(__func__, "seq starting", it->c_str(), "not found", 0));
        f_map[*it].make_sacred();
    }
    return ret;
}

/* ---------------- decider functions ------------------------
 * We have several possibilities for deciding which member of a
 * pair to delete. We use one of these, as pointed to by a
 * function pointer.
 */
typedef unsigned char decider_f (const fseq_prop&, const fseq_prop&, default_random_engine&);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static unsigned char
always_first (const fseq_prop &f1, const fseq_prop &f2, default_random_engine &r_engine)
{
    return S_1;
}

static unsigned char
always_second (const fseq_prop &f1, const fseq_prop &f2, default_random_engine &r_engine)
{
    return S_2;
}

static unsigned char
decide_random(const fseq_prop &f1, const fseq_prop &f2, default_random_engine &r_engine)
{
    uniform_int_distribution<int> d{0,1};
    int r = d(r_engine);
    if (r == 1)
        return S_1;
    return S_2;
}

static unsigned char
decide_longer(const fseq_prop &f1, const fseq_prop &f2, default_random_engine &r_engine)
{
    if (f1.ngap < f2.ngap)
        return S_2;
    return S_1;
}
#pragma GCC diagnostic pop
static decider_f *
set_up_choice (const string &s)
{
    map <const string, decider_f *> choice_map;
    choice_map["first"]  = always_first;  /* Table of keywords and */
    choice_map["second"] = always_second; /* function pointers */
    choice_map["random"] = decide_random;
    choice_map["longer"] = decide_longer;
    const map<const string, decider_f *>::const_iterator missing = choice_map.end();
    const map<const string, decider_f *>::const_iterator ent = choice_map.find(s);

    if (ent == missing) {
        cerr << __func__ <<": random choice type \"" << s
             << "\" not known. Stopping. Try one of..\n   ";
        map<const string, decider_f *>::const_iterator it = choice_map.begin();
        for (; it != missing; it++)
            cout << it->first << " " << '\n';
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
choose_seq (const fseq_prop &f1, const fseq_prop &f2, decider_f *choice, default_random_engine &r_engine)
{
    if (f1.is_sacred() && f2.is_sacred())
        return NOBODY;
    if (f1.is_sacred())
        return S_2;
    else if(f2.is_sacred())
        return S_1;
    return (choice (f1, f2, r_engine));
}

/* ---------------- remove_seq -------------------------------
 * Walk down the list of distances, deciding who to delete.
 */
static void
remove_seq (map<string, fseq_prop> &f_map, vector<string> &v_cmt,
            vector<dist_entry> &v_dist, const unsigned long to_keep,
            decider_f *choice, default_random_engine &r_engine)
{
    vector<dist_entry>::const_iterator it = v_dist.begin();
    for ( ; f_map.size() > to_keep  && (it != v_dist.end()); it++) {
        const string &s1 = v_cmt[it->ndx1];
        string &s2 = v_cmt[it->ndx2];
        const map<string, fseq_prop>::const_iterator missing = f_map.end();
        const map<string, fseq_prop>::const_iterator f1      = f_map.find(s1);
        const map<string, fseq_prop>::const_iterator f2      = f_map.find(s2);
        if ((f1 == missing) || (f2 == missing))
            continue;
        switch (choose_seq(f1->second, f2->second, choice, r_engine)) {
        case NOBODY:
            continue;        /* break; otherwise compiler complains */
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
    vector<string>::const_iterator it = v_cmt.begin();
    for (; it != v_cmt.end(); it++)
        if (it->find (SEED_STR) != string::npos)
            f_map.erase (*it);
}

/* ---------------- squash  ----------------------------------
 * Given a string, keep only the positions which are set
 * true in the vector.
 */
static void
squash (string &s, const vector<bool> &v_used) {
    string new_s;
    new_s.reserve(s.size());
    vector<bool>::const_iterator v_it = v_used.begin();
    for( std::string::iterator s_it=s.begin(); s_it!=s.end(); ++s_it, ++v_it)
        if (*v_it)
            new_s += *s_it;
    s = new_s;
    s.shrink_to_fit();
}

/* ---------------- write_kept_seq ---------------------------
 * This is the final writing of sequences that we want to keep.
 */
static int
write_kept_seq (const char *in_fname, const char *out_fname,
                map<string, fseq_prop> &f_map, const vector<bool> &v_used)
{
    ifstream in_file (in_fname);
    const char *o_fail_r = "open fail (reading) on ";
    const char *o_fail_w = "open fail for writing on ";
    bool filter_col;
    if (v_used.size() != 0)
        filter_col = true;
    else
        filter_col = false;

    fseq fs;
    if (! in_file)
        return(bust(__func__, o_fail_r, in_fname, 0));

    ofstream out_file (out_fname);
    if ( ! out_file)
        return (bust(__func__, o_fail_w, out_fname, ": ",  strerror(errno), 0));

    while (fs.fill (in_file, 0)) {
        const map<string, fseq_prop>::const_iterator missing = f_map.end();
        const map<string, fseq_prop>::const_iterator f1 = f_map.find(fs.get_cmmt());
        if (f1 != missing) {
            out_file << fs.get_cmmt() << '\n'; /* Write comment verbatim */
            size_t done = 0, to_go;   /* but the sequence could have long */
            string s = fs.get_seq();  /* lines that should be split into pieces. */
            if (filter_col)           /* Remove columns that were not used */
                squash (s, v_used);
            to_go = s.length();
            while (to_go) {
                size_t this_line = SEQ_LINE_LEN;
                if (SEQ_LINE_LEN > to_go)
                    this_line = to_go;
                out_file << s.substr (done, this_line)<< '\n';
                done += this_line;
                to_go -= this_line;
            }
            f_map.erase (fs.get_cmmt()); /* stop duplicates being written again */
        }
    }

    in_file.close();
    out_file.close();
    return (EXIT_SUCCESS);
}

/* ---------------- find_used_columns ------------------------
 * We have an alignment, but not all the columns are used.
 * Visit every sequence in the alignment
 * Visit every site in the sequence and mark the corresponding
 * position as true if it is not a gap.
 */
static int
find_used_columns (const char *in_fname,
                   const map<string, fseq_prop> &f_map, vector<bool> &v_used,
                   const short unsigned verbosity)
{
    unsigned nf_in = 0;
    ifstream in_file (in_fname);
    fseq fs;
    if (! in_file)
        return (bust(__func__, "open fail reading from ", in_fname, 0));

    const map<string, fseq_prop>::const_iterator missing = f_map.end();
    while (fs.fill (in_file, 0)) {
        nf_in++;
        const map<string, fseq_prop>::const_iterator f = f_map.find(fs.get_cmmt());
        if (f != missing) {
            const string s = fs.get_seq(); /* need this temporary, otherwise memory error */
            string::const_iterator s_it = s.begin();
            vector<bool>::iterator v_it = v_used.begin();
            for (unsigned short n = 0 ;s_it != s.end(); s_it++, v_it++, n++)
                if (*s_it != GAPCHAR)
                    if (! *v_it)
                        *v_it = true;
        }
    }
    in_file.close();
    if (verbosity > 0) {
        unsigned n = 0;
        vector<bool>::iterator v_it = v_used.begin();
        for (; v_it != v_used.end(); v_it++)
            if (*v_it)
                n++;
        cout << __func__<< ": "<< nf_in << " sequences read. Of " <<
            v_used.size() << " sites, "<< n<< " will be kept.\n";
    }
    return EXIT_SUCCESS;
}

/* ---------------- main  ------------------------------------ */
int
main (int argc, char *argv[])
{
    int c;
    short unsigned verbosity = 0;
    bool seedflag   = false;
    bool filter_col = false;
    bool eflag      = false;
    string choice_name, seed_str;
    unsigned long seed;
    default_random_engine r_engine{};
    const char *progname = argv[0];
    const char *sacred_fname = nullptr;

    while ((c = getopt(argc, argv, "a:c:e:fsv")) != -1) {
        switch (c) {
        case 'a':
            sacred_fname = optarg;                                     break;
        case 'c':
            choice_name += optarg;                                     break;
        case 'e':
            seed_str = optarg;                                         break;
        case 'f':
            filter_col = true;                                         break;
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
        return (usage (progname, " too few arguments"));
    const char *in_fname           = argv[optind++];
    const char *dist_fname         = argv[optind++];
    const char *out_fname          = argv[optind++];
    const char *to_keep_str        = argv[optind++];

    unsigned long n_to_keep;
    try {
        n_to_keep = stoul (to_keep_str);
        if (seed_str.length())
            seed = stoul (seed_str);
        else
            seed = DFLT_SEED;
        r_engine.seed( seed);
    } catch (const std::invalid_argument& ia) {
        return(bust(progname, "invalid argument: ", ia.what(), 0));
    }
    cout << progname << ": using " << in_fname << " as multiple seq alignment.\nDistance matrix from "
         << dist_fname << "\nWriting to " << out_fname
         << "\nKeeping " << n_to_keep << " of the sequences\n";

    struct seq_props s_props;
    int gsl_ret;
    thread gsl_thr (get_seq_list, ref(s_props), in_fname, &gsl_ret);

    vector<string> v_sacred;
    int sacred_ret = EXIT_SUCCESS;
    thread sac_thr;
    if (sacred_fname)
        sac_thr = thread (get_sacred, sacred_fname, ref(v_sacred), &sacred_ret);

    decider_f *choice = always_first;

    if (choice_name.size()) {
        if ((choice = set_up_choice(choice_name)) == nullptr) {
            gsl_thr.join();
            sac_thr.join();
            return EXIT_FAILURE;
        }
    }

    vector<dist_entry> v_dist; /* Big vector with sorted distance entries */
    vector<string> v_cmt;

    try {
        read_distmat (dist_fname, v_dist, v_cmt) ;
    } catch (runtime_error &e) {
        gsl_thr.join(); sac_thr.join();
        return (bust(progname, "error reading distance matrix", e.what(), 0));
    }
    gsl_thr.join();
    if (gsl_ret != EXIT_SUCCESS) {
        if (sacred_fname)
            sac_thr.join();
        return (bust(progname, "error in get_seq_list", 0));
    }

    if (check_lists (s_props.f_map, v_cmt) == EXIT_FAILURE) {
        const char *o = "\", original sequences from \"";
        if (sacred_fname)
            sac_thr.join();
        return(bust(progname, "distmat file: \"", dist_fname, o, in_fname, 0));
    }
    if (seedflag)
        remove_seeds (s_props.f_map, v_cmt);
    if (sacred_fname) {
        sac_thr.join();
        if (sacred_ret != EXIT_SUCCESS)
            return (bust(progname, "error reading sacred file from", sacred_fname, 0));
        if (mark_sacred (s_props.f_map, v_sacred) != EXIT_SUCCESS)
            return EXIT_FAILURE;
    }

    remove_seq (s_props.f_map, v_cmt, v_dist, n_to_keep, choice, r_engine);

    vector<bool> v_used;
    if (filter_col) {
        v_used.assign (s_props.len, false);
        find_used_columns (in_fname, s_props.f_map, v_used, verbosity);
    } else {
        v_used.resize(0);
    }

    if (write_kept_seq (in_fname, out_fname, s_props.f_map, v_used) != EXIT_SUCCESS)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
