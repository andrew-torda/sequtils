/*
 * 9 Feb 2016
 * I want to be able to read sequences from a file in fasta format,
 * break them into pieces of length n, and write them to a file, one
 * per line.
 * By default, there will be a special marker for the N- and
 * C-termini.
 */


#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <sstream>

#include "fseq.hh"
#include "bust.hh"

using namespace std;
/* ---------------- structures and constants ----------------- */
static const char NTERM = 'B';  /* Pick these, since they are least */
static const char CTERM = 'U';  /* likely to be used in real sequences */
static const unsigned max_frag_len = 100;

/* ---------------- usage ------------------------------------ */
static int
usage (const char *progname, const char *s)
{
    static const char *u = "infile outfile";
    return (bust (progname, s, "\n", progname, u, 0));
}

/* ---------------- get_next_seq ----------------------------- */
static int
get_next_seq (ifstream &infile, string &s_seq)
{

    try {
        class fseq fseq (infile, 0);
        fseq.clean(false, false); /* Remove gap characters */
        s_seq = NTERM + fseq.get_seq() + CTERM;
        if (s_seq.length() == 2)
            return EXIT_FAILURE;
    } catch (...) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* ---------------- get_frag --------------------------------- */
static int
get_frag (ifstream &infile, const unsigned len_frag, string &s)
{
    static string s_seq;
    static size_t n_last = 0;
    static size_t n = 0;
    while (n >= n_last) {  /* have to get a new sequence */
        if (get_next_seq (infile, s_seq) == EXIT_FAILURE)
            return EXIT_FAILURE;
        n = 0;
        auto t = s_seq.length();
        if (t < len_frag + 1)  /* Check for sequence too short */
            n_last = 0;
        else
            n_last = s_seq.length() - len_frag + 1;
    }
    string t = s_seq.substr (n++, len_frag); /* just the relevant substring */
    s.clear();
    for (unsigned j = 0; j < len_frag; j++) {
        s += char(toupper(t[j]));
        s += ' ';
    }
    return EXIT_SUCCESS;
}

/* ---------------- main  ------------------------------------ */
int
main (int argc, char *argv[])
{
    const char *open_fail_rd  = "open fail for reading on";
    const char *open_fail_wrt = "open fail for writing on";
    const char *conv_fail_int = "conversion error, to int from \"";
    if (argc < 4)
        return usage(argv[0], "too few args");

    const char *progname  = argv[0];
    const char *in_fname  = argv[1];
    const char *out_fname = argv[2];
    const char *fraglen_s = argv[3];
    unsigned len_frag;         /* Fragment length */
    istringstream (fraglen_s) >> len_frag;
    if (len_frag > max_frag_len || len_frag == 0)
        return (bust(progname, conv_fail_int, fraglen_s, "\"",0));

    ifstream infile (in_fname);
    if (!infile)
        return(bust(progname, open_fail_rd, in_fname, strerror(errno), 0));
    ofstream outfile (out_fname);
    if (!outfile)
        return(bust(progname, open_fail_wrt, out_fname, strerror(errno), 0));
    unsigned n = 0;
    for (string s; get_frag (infile, len_frag, s) != EXIT_FAILURE; n++)
        outfile << s << '\n';
    if (n == 0)
        return (bust(progname, "no fragments found in", in_fname, 0));
    else
        cout << "Wrote "<< n << " fragments of length "<< len_frag << " to "<< out_fname<< '\n';
    return EXIT_SUCCESS;
}
