/*
 * 22 oct 2015
 * Read a distance matrix that was written by mafft.
 * The nice thing is that we do not have to worry about the tricky
 * indexing into a half array. We only want to store distances and the
 * relevant indices. It is this list we are going to sort.
 *
 * There are three things we have to read.
 * 1. The numbers at the start tell us how many sequences are coming.
 * 2. A list of sequence comments. We will use these later for
 * indexing.
 *    - store this in a vector.
 * 3. Distances. These go into a flat list called dist_entry's.
 *    - this also goes into a single vector
 * If we have a problem, throw an error. 
 * We do this as a first call and then two object substantiations.
 * mafft lines look like:
   4. =_seed_2a2lA | 7 - 145
   5. = AAD39014.1 PduO [Salmonella enterica subsp. enterica serovar Typhimurium]
 * so  we look for the string ". =".
 */

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <exception> // Probably also only during debugging
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>

#include "distmat_rd.hh"
#include "mgetline.hh"
#include "prog_bug.hh"

using namespace std;

/* ---------------- structures and constants ----------------- */
static const char *NDX_STR = ". =";

/* ---------------- read_info  -------------------------------
 * We have bundles of sequences in a vector. Pull each from
 * the queue
 */
static unsigned
read_info (ifstream &infile, const char *dist_fname){
    stringstream errmsg (string (__func__));
    errmsg << ": reading from " + string (dist_fname) + string (", ");
    unsigned long i, nseq; /* excessive, but correct for stoul */
    string t;
    mgetline (infile, t);
    if ((i = stoul(t)) != 1) {
        cerr << errmsg << "expected 1 on first line, got" + to_string(i) + string ("\n");
        return 0;
    }
    mgetline (infile, t);
    nseq = stoul(t);
    if (nseq > 10000000) {
        cerr << errmsg << ". Broken. nseq seems to be "<<nseq<<'\n';
        return 0;
    }
    
    mgetline (infile, t); /* there is a float we do not use */
    if (nseq > numeric_limits<unsigned>::max())
        prog_bug (__FILE__, __LINE__, "too many sequences");
    return (unsigned)nseq;
}

struct int_comment {
    string cmmt;
    unsigned long i;
};

/* ---------------- split_cmmt  ------------------------------
 * given a sequence line from the mafft distance matrix file,
 * get out the index and the comment.
 */
static struct int_comment
split_cmmt (const string s) {
    stringstream errmsg; errmsg << __func__;
    string::size_type n;
    struct int_comment i_c;
    static const string::size_type i_off = string (NDX_STR).size();
    n = s.find (NDX_STR);
    if (n == string::npos) {
        errmsg << ": broke looking for index in "<< s;
        throw runtime_error (errmsg.str());
    }
    i_c.i = stoul (s.substr(0, n));
    i_c.cmmt = s.substr (n + i_off);
    return i_c;
}


/* ---------------- read_mafft_seq ---------------------------
 */
static int
read_mafft_seq (ifstream &infile, vector<string> &v_cmt,
                const char *dist_fname, const unsigned nseq)
{
    stringstream errmsg (string (__func__));
    errmsg << ": reading from " << string(dist_fname) << string ("\n");
    string t;
    unsigned n = 0;
    v_cmt.reserve (nseq);
    for (unsigned i = 1; i <= nseq; i++) {  /* starting from 1 */
        if (!mgetline (infile, t)) {         /* allows the check below */
            std::cerr << errmsg << "error reading line\n";
            return EXIT_FAILURE;
        }
        struct int_comment i_c = split_cmmt (t);
        if (i_c.i != ++n) {
            std::cerr << errmsg << "Did not get expected integer " << n<< "\n";
            return EXIT_FAILURE;
        }
        v_cmt.push_back (string (">") + i_c.cmmt);
    }
    v_cmt.shrink_to_fit();
    return EXIT_SUCCESS;
}

/* ---------------- read_mafft_dist --------------------------
 */
static int
read_mafft_dist (ifstream &infile, vector<struct dist_entry> &v_dist,
                const char *dist_fname, const unsigned nseq)
{
    stringstream errmsg (string(__func__));
    errmsg << ": reading from " << string(dist_fname) << "\n";
    size_t ntmp = nseq * (nseq - 1) / 2;
    v_dist.reserve (ntmp);
    for (unsigned i = 0; i < nseq ; i++) {
        for (unsigned j = i+1; j < nseq; j++) {
            struct dist_entry d_e;
            float f;
            size_t z = 0;
            char s[20];
            infile >> s;
            f = stof (s, &z);
            d_e = {f, i, j};
            v_dist.push_back(d_e);
        }
    }   
    v_dist.shrink_to_fit();
    return EXIT_SUCCESS;
}

#ifdef boring_version_of_compare
/* ---------------- dist_ent_cmp -----------------------------
 */
static const bool
dist_ent_cmp (const struct dist_entry &a, const struct dist_entry &b)
{ return (a.dist < b.dist); }
#endif /* boring_version_of_compare */
/* ---------------- read_distmat -----------------------------
 */
int
read_distmat (const char *dist_fname, vector<dist_entry> &v_dist, vector<string> &v_cmt)
{
    string errmsg = __func__;
    errmsg += ": reading from " + string (dist_fname) + string (", ");
    ifstream infile (dist_fname);
    if (!infile) {
        errmsg += ": opening " + string (dist_fname)
            + ": " + strerror(errno) + "\n";
        throw runtime_error (errmsg);
    }
    unsigned nseq;
    if ((nseq = read_info (infile, dist_fname)) == EXIT_FAILURE) {
        errmsg += "failed reading info";
        throw runtime_error (errmsg);
    }
    if (read_mafft_seq (infile, v_cmt, dist_fname, nseq) == EXIT_FAILURE)
        return EXIT_FAILURE;

    if (read_mafft_dist (infile, v_dist, dist_fname, nseq) == EXIT_FAILURE)
        return EXIT_FAILURE;
    infile.close();

    std::sort( v_dist.begin(), v_dist.end(),
               [](const struct dist_entry &a, const struct dist_entry &b)
               { return a.dist < b.dist; } );

    return EXIT_SUCCESS;
}

/* ---------------- dist_mat as class ------------------------
 * Above, I wrote C.
 * Now I will make a dist_mat class. This version has everything.
 * It will be a bit memory hungry, but I need the name to
 * index mapping as well as the distance matrix.
 */
dist_mat::dist_mat (const char *dist_fname)
{
    string errmsg = __func__;
    errmsg += ": reading from " + string (dist_fname) + string (", ");
    if (read_distmat (dist_fname, v_dist, v_cmt) == EXIT_FAILURE)
        throw runtime_error (errmsg);
}

/* ---------------- get_dist    ------------------------------
 * Return the distance between the two named entries.
 * Numbering is from zero up.
 * This may be very slow, but we only do this for a few distances
 * right at the end of the procedure.
 */
float
dist_mat::get_pair_dist (const unsigned node1, const unsigned node2) const
{
    if (node1 == node2) return 0.0;
    vector<dist_entry>::const_iterator d_it  = v_dist.begin();
    const vector<dist_entry>::const_iterator d_end = v_dist.end();
    for ( ; d_it < d_end; d_it++) {
        unsigned ndx1 = d_it->ndx1;
        unsigned ndx2 = d_it->ndx2;
        if ( ((node1 == ndx1) && (node2 == ndx2)) ||
             ((node1 == ndx2) && (node2 == ndx1)))
            return d_it->dist;
    }
    string s = "Distance not found in dist mat, node indices: ";
    s += to_string(node1) + ' ' + to_string (node2);
    prog_bug (__FILE__, __LINE__, s.c_str());
}
