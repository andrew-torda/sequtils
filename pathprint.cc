/* 3 jan 2015
 * Print out the path from src to dest.
 * As we get the path, it is in reverse order.
 */

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "bust.hh"
#include "distmat_rd.hh"
#include "filt_string.hh"
#include "fseq.hh"
#include "graphmisc.hh"
#include "pathprint.hh"
#include "seq_index.hh"

using namespace std;

/* ---------------- Structures and constants -----------------
 */
static const float BAD_DIST = -1.0;
static const unsigned SEQ_LINE_LEN = 60;


/* ---------------- path::path -------------------------------
 */
path::path (const vector<src_dist_t> &src_dist,
            const unsigned src, const unsigned dst, const dist_mat &d_m)
{
    n_mbr = 0;
    {
        unsigned node = dst;
        unsigned prev;
        do {
            struct node n_tmp;
            n_tmp.label    = src_dist[node].label;
            n_tmp.src_dist = src_dist[node].dist;
            n_tmp.p_dist   = src_dist[node].p_dist;
            n_tmp.orig_dst_dist = d_m.get_pair_dist (n_tmp.label, src_dist[dst].label);
            n_tmp.orig_src_dist = d_m.get_pair_dist (n_tmp.label, src_dist[src].label);
            if (node == src) {
                n_tmp.pred_label = src_dist[src].label;
            } else {
                unsigned pred = src_dist[node].pred;
                n_tmp.pred_label = src_dist[pred].label;
            }

            nodes.push_back(n_tmp);
            prev = node;
            node = src_dist[node].pred;
        } while (src_dist[prev].pred != INVALID_NODE);
    }
    vector<struct node>::iterator n_it = nodes.begin();
    float sum = 0;
    for ( ; n_it < nodes.end(); n_it++) {
        n_it->dst_dist = sum;
        sum += n_it->p_dist;
    }
}
#ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored  "-Wfloat-equal"
#endif /* __clang__ */
static const unsigned BSIZ = 512;
/* ---------------- nice_string ------------------------------
 * should look like
 seq     src    dst   dst   src  dst
  no    dist   dist  prec   o_dist  o_dist
 */
string
path::nice_string (const path::node & node) const
{

    char buf[BSIZ];
    string s;
    float prec_dist = node.p_dist;
    if (prec_dist == BAD_DIST) /* This is the source */
        prec_dist = 0.0;
    snprintf (buf, BSIZ - 1, "%8d %8.3g %8.3g %8.3g %8.3g %8.3g\n",
        node.label + 1, node.src_dist, node.dst_dist, prec_dist, node.orig_src_dist, node.orig_dst_dist);
    return buf;
}

#ifdef __clang__
#    pragma clang diagnostic pop
#endif /* __clang__ */

static const char *txt1 =
"seq no: Number of sequence in original file\n\
src dist: distance along graph to source sequence\n\
dst dist: distance along graph to destination sequence\n\
dst prec: distance to preceding node\n\
src o_dist: distance to source in original distance matrix\n\
dst o_dist: distance to destination in original distance matrix \n";

/* ---------------- path::print ------------------------------
 * Write a little report about the path we have.
 * I have suffered under the tyranny of c++'s << long enough.
 * We have the src_dist, dst_dist and p_dist.
 */
int
path::print (const char *outfile, const dist_mat &d_m) const
{
    ostream *ofs_p;
    ofstream ofs;
    ofs_p = &cout;
    char buf[BSIZ];
    if (outfile) {
        ofs.open (outfile, ofstream::out);
        if (ofs.fail())
            return EXIT_FAILURE;
        ofs_p = &cout;
    }
    unsigned src_label = nodes.back().label + 1;
    unsigned dst_label = nodes.front().label + 1;
    *ofs_p << "The path went from seq "<< src_label << " to seq " << dst_label<< '\n';

    *ofs_p << txt1;
    const char *txtfmt = "%8s %8s %8s %8s %8s %8s\n";
    snprintf (buf, BSIZ-1, txtfmt, "seq", "src", "dest", "prec", "src", "dst");
    *ofs_p << buf;
    snprintf (buf, BSIZ-1, txtfmt, "no", "dist", "dist", "dist", "o_dist", "o_dist");
    *ofs_p << buf;

    vector<struct node>::const_reverse_iterator v_it = nodes.rbegin();

    for( ;v_it != nodes.rend(); v_it++) {
        string s = nice_string (*v_it);
        *ofs_p << s;
    }

    *ofs_p << "The set of sequences on the path was\n";
    for ( v_it = nodes.rbegin(); v_it != nodes.rend(); v_it++) {
        snprintf (buf, BSIZ-1, "%6d ", v_it->label + 1);
        *ofs_p << buf << d_m.get_cmt(v_it->label) << '\n';
    }
    vector<node> n_copy = nodes;

    std::sort( n_copy.begin(), n_copy.end(),
               [](const struct node &a, const struct node &b)
               { return a.p_dist < b.p_dist; } );
    *ofs_p << "Sorted by distance. These are the weakest links:\n";

    for (v_it = n_copy.rbegin(); v_it < n_copy.rend() - 1 ; v_it++)
        *ofs_p << "Seq " << v_it->label + 1 << " to "<< v_it->pred_label + 1
               << " dist "<< v_it->p_dist<< '\n';
    return EXIT_SUCCESS;
}

/* ---------------- path::write_seqs -------------------------
 * Write the sequences on the path to the output file name.
 * We cannot really re-use the function for writing larger sets
 * of sequences to a file, since the order here is important.
 * We want to be able to look at the file and follow the changes
 * of residues.
 */
int
path::write_seqs (const char *seq_out_fname, const dist_mat &d_m, seq_index &s_i)
{
    ofstream outfile (seq_out_fname);
    if (!outfile)
        return (bust(__func__, "Open fail", s_i.get_fname().c_str(),  ": ", strerror(errno), 0));
    size_t first_size;
    bool do_remove_columns = true; /* Remove completely empty columns from alignment */
    vector<fseq> path_seqs;
    static const char *err1 =
"Sequences in the sequence file have different sizes.\n\
This is probably not what you want. Maybe you should delete all output and check that\n\
you are using sequences after the alignment, not before.\n";


 
    vector<struct node>::const_reverse_iterator v_it = nodes.rbegin();
    first_size = s_i.get_seq_by_cmmt(d_m.get_cmt(v_it->label)).get_size();
    vector<bool> v_c_used(first_size, false);

    try {  /* Next loop visits each node on the path and stores the sequence. */
        for (; v_it != nodes.rend(); v_it++) {     /* We also check if the size is */
            string s = d_m.get_cmt(v_it->label);   /* always the same. */
            fseq fs = s_i.get_seq_by_cmmt(s);      
            if ((fs.get_size() != first_size) && do_remove_columns ) {
                cerr << err1;
                cerr << "Reading sequences from: "<< s_i.get_fname()
                     << " First sequence length: "<< first_size
                     << " This sequence length: " << fs.get_size()
                     << "\nWriting sequences anyway.\n";
                v_c_used.clear();
                do_remove_columns = false;
            }
            path_seqs.push_back(fs);
        }
    } catch (runtime_error &e) {
        cerr << "Trying to look up sequences " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    /* The sequences on the path are now stored in path_seqs. If they were all the same
     * length, they probably came from an alignment, so we can remove completely
     * empty columns
     */
    if (do_remove_columns) {
        vector<fseq>::iterator f_it = path_seqs.begin();
        for ( ; f_it != path_seqs.end(); f_it++) /* Build a vector which tells us which */
            set_vec (f_it->get_seq(), v_c_used); /* columns are used (not gaps) */
        for (f_it = path_seqs.begin(); f_it != path_seqs.end(); f_it++) {
            string s = f_it->get_seq();
            squash_string_vec (s, v_c_used);
            f_it->replace_seq(s);
        }
    }
    {
        vector<fseq>::iterator f_it = path_seqs.begin();
        for ( ; f_it != path_seqs.end(); f_it++)
            f_it->write(outfile, SEQ_LINE_LEN);
    }
    outfile.close();
    return EXIT_SUCCESS;
}


#ifdef want_path_on_path
/* ---------------- path::on_path ----------------------------
 * We are given a vector of length of all the sequences.
 * Mark elements as true if they are on the path.
 */
void
path::on_path (vector<bool> &v_on_path)
{
    v_on_path.assign (v_on_path.size(), false);
    vector<struct node>::const_iterator v_it = nodes.begin();
    for ( ; v_it != nodes.end(); v_it++)
        v_on_path[v_it->label] = true;
}
#endif /* want_path_on_path */
