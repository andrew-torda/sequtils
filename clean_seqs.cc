/* 1 Dec 2015
 * Given a set of sequences, clean them up before aligning them.
 * remove any white space, optionally gaps, any small or large sequences.
 * Calculate the mean and median lengths.
 * Write the output to a new file.
 */

#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
/* #include <regex> gcc 4.8 version is broken, so we use boost:: below */
#include <string>
#include <unordered_set>
#include <cerrno>
#include <unistd.h>  /* for getopt() */
#include <boost/regex.hpp> /* when we update gcc, get rid of this */


#include "bust.hh"
#include "fseq.hh"
#include "mgetline.hh"
#include "t_queue.hh"
using namespace std;

static void breaker(){}

/* ---------------- constants and structures ----------------- */
static const unsigned CLN_QBUF = 101;
static const char COMMENT_CHAR = '#';
struct stats {
    string cmmt_short;
    string cmmt_long;
    string::size_type len_short;
    string::size_type len_long;
    string::size_type ndx_short;
    string::size_type ndx_long;
    string::size_type median;
    float mean;
    float std_dev;
};
struct criteria {
    string::size_type min_len;
    string::size_type max_len;
    float s_arg;  /* These come from the command line */
    float t_arg;  /* Arguments */
};

struct seq_tag {
    boost::regex pattern;
    string comment;
};

struct result {        /* Used by the regex matching */
    long length;       /* functions.  */
    string seq_cmmt;
    string comment;
    string matched;
    unsigned n;
    int nothing; /* get rid of padding warning */
};

/* ---------------- usage ------------------------------------ */
static int
usage ( const char *progname, const char *s)
{
    static const char *u
        = " [-n -s small_seq -t big_seq -g -i min_len -j max_len] [-w warning_tags] seq_tags_fname in_file out_file\n";
    bust_void (progname, s, 0);
    return (bust ("Usage", progname, u, 0));
}

/* ---------------- keep_seq  --------------------------------
 * Given a sequence, return true if he is OK to keep, that is,
 * he is not too short or too long.
 */
static bool
keep_seq ( fseq &f, const struct criteria *criteria)
{
    const string::size_type len = f.get_seq().size();
    if (!criteria)
        return true;
    if (criteria->max_len && len > criteria->max_len)
        return false;
    if (criteria->min_len && len < criteria->min_len)
        return false;
    return true;
}

/* ---------------- regex_check  -----------------------------
 * This looks for a match of a regex in the string. It can
 * get called in two contexts.
 * 1. When we want to do a replacement
 * 2. When we want to warn that something is there.
 * Whatever the story, we send a "result" structure back.
 */
static bool
regex_check (const vector<seq_tag> &v_seq_tag, vector<struct result> &v_res,
             fseq &f, const bool do_replace, const unsigned seq_num)
{
    vector<seq_tag>::const_iterator v_it = v_seq_tag.begin();
    bool match = false;
    string s = f.get_seq();
    for (; v_it != v_seq_tag.end(); v_it++) {
        boost::smatch sm;
        if (boost::regex_search (s, sm, v_it->pattern)) {
            match = true;
            struct result result;
            result.matched = sm[0];
            result.n = seq_num;
            result.length = sm[0].length();
            result.comment = v_it->comment;
            result.seq_cmmt = f.get_cmmt().substr (0, 20);
            v_res.push_back(result);
            if (do_replace)
                s.erase (unsigned(sm.position()), unsigned (sm.length()));
        }
    }
    if (match) {
        breaker();
        f.replace_seq (s);
    }
    return match;
}

/* ---------------- tag_remover  -----------------------------
 * For each sequence, check against the set of tags and
 * remove them.
 */
static void
tag_remover (vector<seq_tag> &v_seq_tag, vector<seq_tag> &v_warn_tag,
             t_queue<fseq> &tag_rmvr_in_q, t_queue<fseq> &tag_rmvr_out_q,
             const struct criteria *criteria)
{
    unsigned seq_num = 0;
    vector <struct result> v_res;
    vector <struct result> v_warn;
    while (tag_rmvr_in_q.alive()) {
        seq_num++;
        fseq f = tag_rmvr_in_q.front_and_pop();
        /* here, decide if we should filter him, based on size */
        if (keep_seq (f, criteria)) {
            regex_check (v_seq_tag, v_res, f, true, seq_num);
            regex_check (v_warn_tag, v_warn, f, false, seq_num);
            tag_rmvr_out_q.push (f);
        } else {
            cout << __func__ << " not keeping seq no. "<< seq_num<< " name "
                 << f.get_cmmt().substr(0,20) << " length "<< f.get_seq().size()<<'\n';
        }
    }
    tag_rmvr_out_q.close();
    cout << __func__<< ": got "<< seq_num << " sequences\n";
    vector<struct result>::const_iterator v_it = v_res.begin();
    for (; v_it != v_res.end(); v_it++)
        cout << __func__<< ": Sequence "<< v_it->n << " starting with \""<< v_it->seq_cmmt
             << "\"\nRemoved \"" << v_it->matched<< "\" (" << v_it->length<< " residues) "
             << v_it->comment << '\n';
    v_it = v_warn.begin();
    for (; v_it != v_warn.end(); v_it++)
        cout << __func__<< ": Sequence "<< v_it->n << " starting with \""<< v_it->seq_cmmt
             << "\" matched \"" << v_it->matched<< "\" (" << v_it->length<< " residues) "
             << v_it->comment << "\nThis is a warning, it was not removed.\n";
}

/* ---------------- clean_one_seq ----------------------------
 */
static string
clean_one_seq (const string &s, const bool keep_gap)
{
    static const boost::regex white("\\s");  /* Here is where we want */
    static const boost::regex gap("-");      /* to move to std library */
    const char *replace = "";
    string new_s = boost::regex_replace (s, white, replace);
    if (! keep_gap)
        new_s = boost::regex_replace (new_s, gap, replace);
    return new_s;
}

/* ---------------- cleaner   --------------------------------
 * We have sequences, but they might have rubbish like white
 * spaces there.
 * Read from a queue and send the cleaned sequences out.
 * This thread also starts the tag_remover process. We could start
 * the tag_remover in the caller, which would just mean declaring
 * the queue and passing it here as an arguement.
 * Each sequence comment is put in a hash. If a sequence re-occurs
 * we ignore it after the first time.
 */
static void
cleaner (t_queue<fseq> &cleaner_in_q, t_queue<fseq> &tag_rmvr_out_q,
         vector<seq_tag> &v_seq_tag, vector<seq_tag> &v_warn_tag,
         const struct criteria *criteria,
         const bool keep_gap, const unsigned short verbosity)
{
    t_queue<fseq> tag_rmvr_in_q (CLN_QBUF+2);
    string replace = "";              /* when we update gcc */
    unsigned in_n = 0;
    unsigned out_n = 0;
    unordered_set<string> name_hash;
    
    thread tag_rmvr_thr (tag_remover, ref(v_seq_tag), ref(v_warn_tag),
                         ref(tag_rmvr_in_q), ref(tag_rmvr_out_q), criteria);
    while (cleaner_in_q.alive()) {
        fseq f = cleaner_in_q.front_and_pop();
        in_n++;
        if (name_hash.find (f.get_cmmt()) == name_hash.end())  {
            name_hash.insert (f.get_cmmt());
            f.clean(keep_gap);
            tag_rmvr_in_q.push (f);
            out_n++;
        } else {
            cout << "duplicate: " << f.get_cmmt().substr (0,25) << "\n";
        }
    }
    tag_rmvr_in_q.close();
    tag_rmvr_thr.join();
    if (verbosity > 1)
        cout << __func__<< ": got "<< in_n << " seqs. Wrote "<< out_n << " seqs\n";
}


/* ---------------- read_get_stats ---------------------------
 * This runs in parallel with the slower queue system. Grab the
 * statistics as quickly as possible so they are ready for
 * the writer process.
 */
static void
read_get_stats (const char *in_fname, struct stats *stats, int *ret)
{
    string errmsg = __func__;
    vector<string::size_type> v_lens;
    string cmmt_short, cmmt_long;
    string::size_type nsum = 0,
                      nsum_sq = 0,
                      nlong = 0,
                      nshort,
                      nseq = 0,
                      ndx_short,
                      ndx_long;
    {
        string t;
        nshort = t.npos;
    }

    ifstream infile (in_fname);

    if (!infile) {
        *ret = EXIT_FAILURE;
        return (bust_void (__func__, "opening", in_fname, ": ", strerror(errno), 0));
    }
    ndx_short = ndx_long = 0;  /* stops compiler warnings */
    for (fseq fs; fs.fill(infile, 0); nseq++) {
        string s = clean_one_seq (fs.get_seq(), false);
        string::size_type n = s.size();
        if (n < nshort) {
            nshort = n;
            cmmt_short = fs.get_cmmt();
            ndx_short = nseq + 1;
        }
        if (n > nlong) {
            nlong = n;
            cmmt_long = fs.get_cmmt();
            ndx_long = nseq + 1;
        }
        nsum += n;
        nsum_sq += n * n;
        v_lens.push_back (n);
    }
    infile.close();
    if (nseq < 1) {
        *ret = EXIT_FAILURE;
        return (bust_void (__func__, "reading from", in_fname, "got no sequences\n", 0));
    }
    stats->cmmt_short = cmmt_short;
    stats->cmmt_long  = cmmt_long;
    stats->len_short  = nshort;
    stats->len_long   = nlong;
    stats->ndx_short  = ndx_short;
    stats->ndx_long   = ndx_long;

    const string::size_type n = v_lens.size();
    stats->mean = float (nsum) / float (n);
    vector<string::size_type>::difference_type non2 = n / 2; /* deliberate integer division */
    nth_element(v_lens.begin(), v_lens.begin() + non2, v_lens.end());
    stats->median = v_lens[unsigned(non2)];


    float p1 = float (1.0) / (n *(n-1));
    unsigned long p2 = n * nsum_sq - (nsum * nsum);
    stats->std_dev = sqrt (p1 * p2);
}
/* ---------------- read_seqs --------------------------------
 * Read sequences, remove white space and pass them on to
 * queues for more processing.
 * Return the number of sequences read.
 */
static unsigned
read_seqs (const char *in_fname, t_queue<fseq> &tag_rmvr_out_q,
           vector<seq_tag> v_seq_tag, vector<seq_tag> v_warn_tag,
           const struct criteria *criteria,
           const bool keep_gap, const unsigned short verbosity)
{
    string errmsg = __func__;
    ifstream infile (in_fname);
    unsigned nseq = 0;

    if (!infile) {
        bust_void (__func__, "opening ", in_fname, ": ", strerror(errno), 0);
        return 0;
    }

    t_queue<fseq> cleaner_in_q (CLN_QBUF);
    t_queue<fseq> tag_rmvr_in_q (CLN_QBUF - 1);

    thread cln_thrd (cleaner, ref(cleaner_in_q), ref(tag_rmvr_out_q),
                     ref(v_seq_tag), ref(v_warn_tag), criteria, keep_gap, verbosity);

    for ( fseq fs; fs.fill(infile, 0); nseq++)
        cleaner_in_q.push(fs);
    cleaner_in_q.close();
    cln_thrd.join();
    infile.close();
    if (verbosity > 0)
        cout << __func__<< ": read "<< nseq << " sequences\n";
    return nseq;
}

/* ---------------- split_to_tag -----------------------------
 * Given a line, turn the first part into a regex.
 * Turn the second part into an comment. The line,
 *            h{5} information about the tag
 * becomes "h{5}" and "information about the tag".
 */
static void
split_to_tag ( string &s, struct seq_tag *s_t)
{
    string tmp;
    string::size_type pos = s.find(' ');
    if (pos == s.npos)
        tmp = "";
    else
        tmp = s.substr (pos);
    tmp = boost::regex_replace(tmp, boost::regex("^\\s+"), ""); /* remove leading spaces */
    s_t->comment = tmp;
    s.resize(pos);
    boost::regex r(s);
    s_t->pattern = boost::regex(s, boost::regex::icase|boost::regex::optimize);
}


/* ---------------- read_tags --------------------------------
 * Read the list of tags we want to remove or warn about.
 */
static int
read_tags (const char *seq_tags_fname, vector<seq_tag> &v_seq_tag)
{
    string errmsg = __func__;
    ifstream infile (seq_tags_fname);
    unsigned ntag = 0;
    if (! infile) {
        errmsg += ": opening " + string (seq_tags_fname) + ": " + strerror(errno) + '\n';
        cerr << errmsg;
        return EXIT_FAILURE;
    }
    string s;
    while (mc_getline (infile, s, COMMENT_CHAR)) {
        struct seq_tag s_t;
        split_to_tag (s, &s_t);
        v_seq_tag.push_back (s_t);
        ntag++;
    }
    infile.close();
    cout << __func__ << ": read "<<ntag << " tags\n";
    return EXIT_SUCCESS;
}

/* ---------------- seq_writer -------------------------------
 * Read from a queue and write to our output file.
 */
static void
seq_writer (const char *seq_tags_fname, t_queue<fseq> &tag_rmvr_out_q,
            const short unsigned verbosity, const bool nothing_flag, int *ret)
{
    string errmsg = __func__;
    *ret = EXIT_SUCCESS;
    unsigned n_in = 0, n_out = 0;
    ofstream outfile (seq_tags_fname);
    if (! outfile) {
        errmsg += ": opening " + string (seq_tags_fname) + ": " + strerror(errno) + '\n';
        cerr << errmsg;
        *ret = EXIT_FAILURE;
        while (tag_rmvr_out_q.alive())
            tag_rmvr_out_q.front_and_pop();
        return;
    }

    while (tag_rmvr_out_q.alive()) {
        fseq f = tag_rmvr_out_q.front_and_pop();
        n_in++;
        if (! nothing_flag) {
            f.write (outfile, 70);
            n_out++;
        }
    }
    outfile.close();
    if (verbosity > 0)
        cout << __func__<< ": received " << n_in << " sequences and wrote "
             << n_out << " of them\n";
}

/* ---------------- set_criteria_cmd_ln ----------------------
 * Set some variables from command line options.
 */
static int
set_criteria_cmd_ln (struct criteria *criteria, const char *small_seq_str, const char *big_seq_str)
{
    bool e = false;
    if (small_seq_str) {
        try {
            criteria->min_len = stoul (small_seq_str, nullptr, 0);
        } catch (const std::invalid_argument& ia) {
            std::cerr << "Invalid argument: " << ia.what() <<
                "\nwhen converting "<< small_seq_str << " for min number residues\n";
            e = true;
        }
    }
    if (big_seq_str) {
        try {
            criteria->max_len = stoul (big_seq_str, nullptr, 0);
        } catch (const std::invalid_argument& ia) {
            std::cerr << "Invalid argument: " << ia.what() <<
                "\nwhen converting "<< big_seq_str << " for max number residues\n";
            e = true;
        }
    }
    if (e)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

/* ---------------- set_criteria_std_dev --------------------- */
static int
set_criteria_std_dev (struct criteria *criteria, const struct stats *stats,
                      const char *small_seq_str, const char *big_seq_str)
{
    if (small_seq_str) {
        float f = stof(small_seq_str);
        criteria->min_len = stats->median - unsigned (f * stats->std_dev);
        cout<< "Set minimum seq length to "<< criteria->min_len << '\n';
    }
    if (big_seq_str) {
        float f = stof(big_seq_str);
        criteria->max_len = stats->median + unsigned (f * stats->std_dev);
        cout<< "Set max seq length to "<< criteria->max_len << '\n';
    }
    return EXIT_SUCCESS;
}

/* ---------------- main  ------------------------------------ */
int
main (int argc, char *argv[])
{
    struct stats stats;
    unsigned nseq;
    int c;
    const char *progname = argv[0],
               *small_seq_str = NULL,
               *big_seq_str = NULL,
               *in_fname,
               *out_fname,
               *min_seq_str = NULL,
               *max_seq_str = NULL,
               *seq_tags_fname,
               *warn_tags_fname = NULL;
    short unsigned verbosity = 0;
    bool  keep_gap = false,
          nothing_flag = false,
          eflag = false;
    struct criteria *crit_ptr = NULL;
    struct criteria criteria = {0, 0, 0, 0};
    while ((c = getopt(argc, argv, "gi:j:ns:t:vw:")) != -1) {
        switch (c) {
        case 'g': keep_gap = true;                                     break;
        case 'n': nothing_flag    = true;                              break;
        case 'i': min_seq_str     = optarg;                            break;
        case 'j': max_seq_str     = optarg;                            break;
        case 's': small_seq_str   = optarg;                            break;
        case 't': big_seq_str     = optarg;                            break;
        case 'v': verbosity++;                                         break;
        case 'w': warn_tags_fname = optarg;                            break;
        case ':':
            cerr << argv[0] << " Missing opt argument\n"; eflag = true; break;
        case '?':
            cerr << argv[0] << " Unknown option\n";       eflag = true; break;
        }
    }
    if (eflag)
        return (usage(progname, ""));
    if ((argc - optind) < 3)
        return (usage(progname, " too few arguments"));

    seq_tags_fname = argv[optind++];
    in_fname       = argv[optind++];
    out_fname      = argv[optind++];


    if (min_seq_str || max_seq_str) {
        if (set_criteria_cmd_ln ( &criteria, min_seq_str, max_seq_str ) != EXIT_SUCCESS)
            return EXIT_FAILURE;
        crit_ptr = & criteria; /* Use this later to say if criteria have been set */
    }
    cout << progname << " with seq tags read from " << seq_tags_fname
         << "\nInput sequences from "<< in_fname << ".\nWriting output sequences to "
         << out_fname << "\n\n";
    if (warn_tags_fname)
        cout << "Unhappy strings for warnings will be read from " << warn_tags_fname << '\n';
    if (keep_gap )
        cout << "Gaps will not be deleted from the input sequences\n";
    if (nothing_flag)
        cout << "No output will be written to the new sequence file\n";


    int stat_ret;
    thread stat_thrd (read_get_stats, in_fname, &stats, &stat_ret);
    /* The first pass over the sequences has started in the background. */
    /* While this is happening, get the sequence tags and build their */
    /* regular expressions. */

    vector<seq_tag> v_seq_tag;
    vector<seq_tag> v_warn_tag;
    if (read_tags (seq_tags_fname, v_seq_tag) == EXIT_FAILURE)
        return EXIT_FAILURE;
    if (warn_tags_fname)
        if (read_tags (warn_tags_fname, v_warn_tag) == EXIT_FAILURE)
            return EXIT_FAILURE;

    t_queue<fseq> tag_rmvr_out_q (CLN_QBUF);

    stat_thrd.join();  /* End of first pass over the sequences */

    if (small_seq_str || big_seq_str) {
        set_criteria_std_dev (&criteria, &stats, small_seq_str, big_seq_str);
        crit_ptr = & criteria;
    }

    int writer_ret;
    thread writer_thrd (seq_writer, out_fname, ref(tag_rmvr_out_q),
                        verbosity, nothing_flag, &writer_ret);

    if ((nseq = read_seqs (in_fname, tag_rmvr_out_q, v_seq_tag,
                           v_warn_tag, crit_ptr, keep_gap, verbosity)) == 0) {
        writer_thrd.join();
        return (bust(__func__, "no sequences in ", in_fname, 0));
    }

    writer_thrd.join();
    if (writer_ret != EXIT_SUCCESS)
        return (bust (progname, "Stopping because of problem with seq_writer", 0));

    cout << fixed<< "There were " << nseq << " sequences. Median length: "<< stats.median
         << " Average length "<< setprecision (1) << stats.mean
         << " +- "<< setprecision (1) << stats.std_dev << " deviation\n";
    if (crit_ptr) {
        if (crit_ptr->min_len)
            cout << "Sequence minimum length to be kept: "<< crit_ptr->min_len<<'\n';
        if (crit_ptr->max_len)
            cout << "Sequence maximum length to be kept: "<< crit_ptr->max_len<<'\n';
    }
    cout << "Shortest sequence no. " << stats.ndx_short << ", length "
         << stats.len_short << " starts with\n"
         << stats.cmmt_short.substr (0,50) << '\n'
         << "Longest sequence no. "  << stats.ndx_long  << ", length "
         << stats.len_long << " starts with \n"
         << stats.cmmt_long.substr (0, 50) << '\n';
    return EXIT_SUCCESS;
}
