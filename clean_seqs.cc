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

#include <cerrno>
#include <unistd.h>  /* for getopt() */
#include <boost/regex.hpp> /* when we update gcc, get rid of this */

#include "fseq.hh"
#include "mgetline.hh"
#include "t_queue.hh"
using namespace std;

static void breaker(){}

/* ---------------- constants and structures ----------------- */
static const unsigned CLN_QBUF = 101;
static const char GAPCHAR = '-';
static const char COMMENT_CHAR = '#';
struct stats {
    string::size_type median;
    float mean;
    float std_dev;
};

struct seq_tag {
    boost::regex pattern;
    string comment;
};

/* ---------------- usage ------------------------------------ */
static int
usage ( const char *progname, const char *s)
{
    static const char *u
        = ": [-n -s small_seq -t big_seq -g] seq_tags_fname in_file out_file\n";
    cerr << progname << s << '\n' << progname << u;
    return EXIT_FAILURE;
}

/* ---------------- stats_collector --------------------------
 * Collect statistics on the sequences.
 * We put the results into the stats structure.
 */
static void
stats_collector (t_queue<fseq> & stat_in_q, struct stats *stats)
{
    vector<string::size_type> v_lens;
    string::size_type nsum = 0,
                      nsum_sq = 0;
    float std_dev;
    while (stat_in_q.alive()) {
        fseq f = stat_in_q.front_and_pop();
        string::size_type tmp = f.get_seq().size();
        nsum += tmp;
        nsum_sq += tmp * tmp;
        v_lens.push_back(tmp);
    }
    const string::size_type n = v_lens.size();
    const string::size_type non2 = n / 2; /* deliberate integer division */
    float p1 = 1.0 / (n *(n-1));
    unsigned long p2 = n * nsum_sq - (nsum * nsum);
    stats->mean = (float) nsum / n;
    stats->std_dev = sqrt (p1 * p2);
    nth_element(v_lens.begin(), v_lens.begin() + non2, v_lens.end());
    stats->median = v_lens[non2];
}

/* ---------------- tag_remover  -----------------------------
 * For each sequence, check against the set of tags and
 * remove them.
 */
static void
tag_remover (vector<seq_tag> &v_seq_tag, t_queue<fseq> &tag_remover_in_q,
             t_queue<fseq> &tag_remover_out_q)
{
    unsigned n = 0;
    struct result {
        unsigned n;
        unsigned length;
        string seq_cmmt;
        string comment;
    };
    vector <struct result> v_res;
    while (tag_remover_in_q.alive()) {
        bool match = false;
        n++;
        fseq f = tag_remover_in_q.front_and_pop();
        string s = f.get_seq();
        vector<seq_tag>::const_iterator v_it = v_seq_tag.begin();
        for (; v_it != v_seq_tag.end(); v_it++) {
            boost::smatch sm;
            if (boost::regex_search (s, sm, v_it->pattern)) {
                match = true;
                struct result result;
                result.n = n;
                result.length = sm[0].second - sm[0].first;
                result.comment = v_it->comment;
                result.seq_cmmt = f.get_cmmt().substr (0, 20);
                v_res.push_back(result);
                s = boost::regex_replace (s, v_it->pattern, "");
            }
        }
        if (match)
            f.replace_seq (s);
        tag_remover_out_q.push (f);
    }
    cout << __func__<< ": got "<< n << " sequences\n";
    vector<struct result>::const_iterator v_it = v_res.begin();
    for (; v_it != v_res.end(); v_it++) {
        cout << __func__<< ": Sequence "<< v_it->n << " starting with "<< v_it->seq_cmmt
             << " removed " << v_it->length<< " residues: "
             << v_it->comment << '\n';
    }
}

/* ---------------- cleaner   --------------------------------
 * We have sequences, but they might have rubbish like white
 * spaces there.
 * Read from a queue and send the cleaned sequences out.
 */
static void
cleaner (t_queue<fseq> &cleaner_in_q, t_queue<fseq> &tag_remover_out_q, vector<seq_tag> &v_seq_tag,
         struct stats *stats, const bool keep_gap, const unsigned short verbosity)
{
    t_queue<fseq> stats_in_q (CLN_QBUF);
    t_queue<fseq> tag_remover_in_q (CLN_QBUF+2);
    const boost::regex white("\\s");  /* Here is where we want */
    const boost::regex gap("-");      /* to move to std library */
    string replace = "";              /* when we update gcc */
    unsigned n = 0;
    thread stats_thr (stats_collector, ref(stats_in_q), stats);
    thread tag_remover_thr (tag_remover, ref(v_seq_tag), ref(tag_remover_in_q), ref(tag_remover_out_q));
    while (cleaner_in_q.alive()) {
        fseq f = cleaner_in_q.front_and_pop();
        string s = f.get_seq();
        string new_s = boost::regex_replace (s, white, replace);
        if (! keep_gap)
            new_s = boost::regex_replace (new_s, gap, replace);
        f.replace_seq (new_s);
        tag_remover_in_q.push (f);
        stats_in_q.push (f);
        n++;
    }
    tag_remover_in_q.close();
    stats_in_q.close();
    stats_thr.join();
    tag_remover_in_q.close();
    tag_remover_thr.join();
    if (verbosity > 1)
        cout << __func__<< ": put "<< n << " sequences on queues\n";
}

/* ---------------- read_seqs --------------------------------
 * Read sequences, remove white space and pass them on to
 * queues for more processing.
 * Return the number of sequences read.
 */
static unsigned
read_seqs (const char *in_fname, struct stats *stats, t_queue<fseq> &tag_remover_out_q,
           vector<seq_tag> v_seq_tag, const bool keep_gap, const unsigned short verbosity)
{
    string errmsg = __func__;
    string::size_type len;
    ifstream infile (in_fname);
    unsigned nseq = 0;

    if (!infile) {
        errmsg += ": opening " + string (in_fname) + ": " + strerror(errno) + '\n';
        cerr << errmsg;
        return EXIT_FAILURE;
    }
    { /* look at the first sequence, get the length, so we can check on */
        streampos pos = infile.tellg();             /* subsequent reads */
        fseq fs(infile, 0);
        len = fs.get_seq().size();
        infile.seekg (pos);
    }
    t_queue<fseq> cleaner_in_q (CLN_QBUF);
    t_queue<fseq> tag_remover_in_q (CLN_QBUF - 1);

    thread cln_thrd (cleaner,
                     ref(cleaner_in_q), ref(tag_remover_out_q), ref(v_seq_tag), stats, keep_gap, verbosity);

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
 * Turn the second part into an comment.
 * h{5} information about the tag
 * becomes "h{5}" and "information about the tag".
 */
static int
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
 * For the moment, this is a free standing function, but we 
 * will change it to a thread with the happiness returned
 * via an in in the argument list.
 */
static int
read_tags (const char *seq_tags_fname, vector<seq_tag> &v_seq_tag)
{
    string errmsg = __func__;
    ifstream infile (seq_tags_fname);
    unsigned ntag;
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
static int
seq_writer (const char *seq_tags_fname, t_queue<fseq> &tag_remover_out_q,
            const short unsigned verbosity, const bool nothing_flag, int *ret)
{
    string errmsg = __func__;
    unsigned n_in = 0, n_out = 0;
    ofstream outfile (seq_tags_fname);
    if (! outfile) {
        errmsg += ": opening " + string (seq_tags_fname) + ": " + strerror(errno) + '\n';
        cerr << errmsg;
        *ret = EXIT_FAILURE;
    }
    while (tag_remover_out_q.alive()) {
        fseq f = tag_remover_out_q.front_and_pop();
        n_in++;
        if (! nothing_flag) {
            f.write (outfile, 60);
            n_out++;
        }
    }
    outfile.close();
    if (verbosity > 0)
        cout << __func__<< ": received " << n_in << " sequences and wrote "
             << n_out << " of them\n";
    *ret = EXIT_SUCCESS;
}

/* ---------------- main  ------------------------------------ */
int
main (int argc, char *argv[])
{
    struct stats stats;
    unsigned nseq;
    int c;
    short unsigned verbosity = 0;
    const char *progname = argv[0],
               *small_seq_str,
               *big_seq_str,
               *in_fname,
               *out_fname,
               *seq_tags_fname;
    int rt_ret;
    bool  keep_gap = false,
          nothing_flag = false,
          eflag = false;
    while ((c = getopt(argc, argv, "gns:t:v")) != -1) {
        switch (c) {
        case 'g': keep_gap = true;                                     break;
        case 'n': nothing_flag = true;                                 break;
        case 's': small_seq_str = optarg;                              break;
        case 't': big_seq_str = optarg;                                break;
        case 'v': verbosity++;                                         break;
        case ':':
            cerr << argv[0] << "Missing opt argument\n"; eflag = true; break;
        case '?':
            cerr << argv[0] << "Unknown option\n";       eflag = true; break;
        }
    }
    if (eflag)
        return (usage(progname, ""));
    if ((argc - optind) < 3)
        return (usage(progname, " too few arguments"));

    seq_tags_fname = argv[optind++];
    in_fname       = argv[optind++];
    out_fname      = argv[optind++];
    cout << progname << " with seq tags read from " << seq_tags_fname
         << " input sequences from "<< in_fname << ". Writing output sequences to "
         << out_fname << '\n';
    if (keep_gap )
        cout << "Gaps will not be deleted from the input sequences\n";
    if (nothing_flag)
        cout << "No output will be written to the new sequence file\n";
    vector<seq_tag> v_seq_tag;
    if (read_tags (seq_tags_fname, v_seq_tag) == EXIT_FAILURE)
        return EXIT_FAILURE;
    t_queue<fseq> tag_remover_out_q (100);
    /* start the writer q */
    int writer_ret;
    thread writer_thrd (seq_writer, out_fname, ref(tag_remover_out_q),
                        verbosity, nothing_flag, &writer_ret);
    if ((nseq = read_seqs (in_fname, &stats, tag_remover_out_q, v_seq_tag, keep_gap, verbosity)) == 0)
        return EXIT_FAILURE;
    
    writer_thrd.join();
    if (writer_ret != EXIT_SUCCESS)
        return EXIT_SUCCESS;
    /* check return from writer */
    cout << fixed<< "There were " << nseq << " sequences. Median length: "<< stats.median
         << " Average length "<< setprecision (1) << stats.mean
         << " +- "<< setprecision (1) << stats.std_dev << " deviation\n";
    return EXIT_SUCCESS;
}
