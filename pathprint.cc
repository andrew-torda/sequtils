/* 3 jan 2015
 * Print out the path from src to dest.
 * As we get the path, it is in reverse order.
 */

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "distmat_rd.hh"
#include "graphmisc.hh"
#include "pathprint.hh"

using namespace std;

/* ---------------- Structures and constants -----------------
 */
static const float BAD_DIST = -1.0;

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
    {
        for (v_it = n_copy.rbegin(); v_it < n_copy.rend() - 1 ; v_it++)
            *ofs_p << "Seq " << v_it->label + 1 << " to "<< v_it->pred_label + 1 << " dist "<< v_it->p_dist<< '\n';
    }
    return EXIT_SUCCESS;
}
