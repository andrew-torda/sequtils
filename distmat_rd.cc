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

#include "bust.hh"
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
    stringstream errmsg (string (__func__) + ": reading distance matrix from " +
                         string (dist_fname) + string (", "));
    unsigned long i, nseq; /* excessive, but correct for stoul */
    string t;
    mgetline (infile, t);
    try {        
        if ((i = stoul(t)) != 1) {
            cerr << errmsg.str() << "expected 1 on first line, got:\n" + to_string(i) + string ("\n");
            return 0;
        }
    } catch (const std::invalid_argument &e) {
        cerr << e.what() << '\n';
        cerr << errmsg.str() << "looking for a single integer, got\n" + t + "\n" +
            __func__ + ": Check if " + dist_fname + " is really a distance matrix file\n";
        return 0;
    }
    
     
    mgetline (infile, t);
    nseq = stoul(t);
    if (nseq > 10000000) {
        cerr << errmsg.str() << ". Broken. nseq seems to be "<<nseq<<'\n';
        return 0;
    }
    
    mgetline (infile, t); /* there is a float we do not use */
    if (nseq > numeric_limits<unsigned>::max())
        prog_bug (__FILE__, __LINE__, "too many sequences");
    return unsigned (nseq);
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
    static const char *err_line = "Error reading line from ";
    string t;
    unsigned n = 0;
    v_cmt.reserve (nseq);
    for (unsigned i = 1; i <= nseq; i++) {  /* starting from 1 */
        if (!mgetline (infile, t))          /* allows the check below */
            return (bust(__func__, err_line, dist_fname, 0));
        struct int_comment i_c = split_cmmt (t);
        if (i_c.i != ++n)
            return (bust(__func__, "Did not get expected integer ", to_string(n).c_str(), 0));
        v_cmt.push_back (string (">") + i_c.cmmt);
    }
    v_cmt.shrink_to_fit();
    return EXIT_SUCCESS;
}

#ifdef use_get_next_float
#include <cstdlib>
/* ---------------- refill_fbuf  -----------------------------
 * read a line and put all the values into a float vector.
 */
static void
refill_fbuf (ifstream &infile, vector<float> &fbuf)
{
    fbuf.clear();
    string s;
    const char *nptr;
    char *endptr;
    mgetline (infile, s);
    endptr = (char *) s.c_str();
    do {  // Stop fighting and change it to while (1) loop
        nptr = endptr;
        float f = strtof(nptr, &endptr);
        if (endptr != nptr)
            fbuf.push_back(f);
    } while (endptr != nptr);
    
    return;
//  float strtof(const char *restrict nptr, char **restrict endptr);
}


/* ---------------- get_next_float  --------------------------
 */
static float
get_next_float (ifstream &infile)
{
    static vector<float> fbuf;  // Move these into the caller and we can avoid statics
    static unsigned used = 0;
    if (used == fbuf.size()) {
        refill_fbuf (infile, fbuf);
        used = 0;
    }
    return fbuf[used++];
}
#endif /* use_get_next_float */

/* ---------------- read_mafft_dist --------------------------
 * This seems to take a lot of time. I used to have
            size_t z = 0; char s[20]; infile >> s; f = stof (s, &z)
 * and it ran just as fast as directly saying infile >> f
 */
static int
read_mafft_dist (ifstream &infile, vector<struct dist_entry> &v_dist,
                const char *dist_fname, const unsigned nseq)
{
    const char *read_err = "Reading error, parsing floats in ";
    size_t ntmp = nseq * (nseq - 1) / 2;
    try {
        v_dist.reserve (ntmp);
    } catch (bad_alloc &e) {
        auto stmp = std::to_string(nseq);
        return (bust(__func__, "Broke reserving space for", stmp.c_str(), "seqs", e.what(), 0));
    }
    try {
        for (unsigned i = 0; i < nseq ; i++) {
            for (unsigned j = i+1; j < nseq; j++) {
                struct dist_entry d_e = { -99, i, j};
#                   ifdef use_get_next_float
                        d_e.dist = get_next_float(infile);
#                   else            
                        infile >> d_e.dist;
#                   endif   /* use_get_next_float */
            v_dist.push_back(d_e);
            }
        }
    } catch (ios_base::failure &e) {
        return (bust(__func__, read_err, dist_fname, "\n", e.what(), 0));
    }
    v_dist.shrink_to_fit();
    return EXIT_SUCCESS;
}


/* ---------------- no_node_in_common ------------------------
 */
static bool
no_node_in_common (const struct dist_entry &a, const struct dist_entry &b)
{
    if ((a.ndx1 == b.ndx1) || (a.ndx1 == b.ndx2) ||
        (a.ndx2 == b.ndx1) || (a.ndx2 == b.ndx2))
        return false;
    return true;
}
/* ---------------- smaller_node_num  ------------------------
 * For an edge, return the node with the smaller number
 */
static unsigned
smaller_node_num (const struct dist_entry d_e)
{
    if (d_e.ndx1 < d_e.ndx2)
        return d_e.ndx1;
    return d_e.ndx2;
}
/* ---------------- get+relevant_pair ------------------------
 * There are four possibilies, so step through them.
 * of the pairs AB and CD, if A is the same as C, we want to
 * return BD and so on.
 */
struct two_u {
    unsigned int a;
    unsigned int b;
};

static struct two_u
get_relevant_pair (const struct dist_entry &a, const struct dist_entry &b)
{
    if (a.ndx1 == b.ndx1)
        return {a.ndx2, b.ndx2 };
    if (a.ndx1 == b.ndx2)
        return {a.ndx2, b.ndx1 };
    if (a.ndx2 == b.ndx1)
        return {a.ndx1, b.ndx2 };
    if (a.ndx2 == b.ndx2)
        return {a.ndx1, b.ndx1};
    prog_bug (__FILE__, __LINE__, "node comparison broke");

}

/* ---------------- dist_ent_cmp -----------------------------
 * Compare distance matrix entries. This is complicated.
 * If the distances are different, do a numeric comparison.
 * If the distances are equal, we have to compare based on
 * node names.
 */
static bool
dist_ent_cmp (const struct dist_entry &a, const struct dist_entry &b)
{
    if (a.dist < b.dist)
        return true;
    if (a.dist > b.dist)
        return false;   /* now the difficult cases */
    if (no_node_in_common (a, b)) {
        if (smaller_node_num (a) < smaller_node_num (b))
            return true;
        else
            return false;
    } else {             /* There is a node in common */
        struct two_u t = get_relevant_pair (a, b);
        return (t.a < t.b);
    }
}


/* ---------------- read_distmat -----------------------------
 */
int
read_distmat (const char *dist_fname, vector<dist_entry> &v_dist, vector<string> &v_cmt)
{
    const char *e_info = "Failed reading info lines from";
    ifstream infile (dist_fname);
    if (!infile)
        return (bust (__func__, "Failed opening ", dist_fname, ": ", strerror(errno), 0));

    unsigned nseq;
    if ((nseq = read_info (infile, dist_fname)) == 0)
        return (bust (__func__, e_info, dist_fname, 0));

    if (read_mafft_seq (infile, v_cmt, dist_fname, nseq) == EXIT_FAILURE)
        return EXIT_FAILURE;

    if (read_mafft_dist (infile, v_dist, dist_fname, nseq) == EXIT_FAILURE)
        return EXIT_FAILURE;
    infile.close();

    std::sort( v_dist.begin(), v_dist.end(), dist_ent_cmp);

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
    if (read_distmat (dist_fname, v_dist, v_cmt) == EXIT_FAILURE) {
        fail_bit = true;
        cerr << string (__func__) + ": reading from " + dist_fname + '\n';
    } else {
        fail_bit = false;
    }
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
