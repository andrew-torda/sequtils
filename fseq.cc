/* 10 oct 2015 */
static const char __attribute__((unused)) *rcsid = "$Id: fseq.cc,v 1.1 2015/10/20 13:14:20 torda Exp torda $";

#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

#include "fseq.hh"
using namespace std;
/* we split this in two parts
 * 1. the comment
 * 2. the sequence.
 * We do not necessarily have a comment and sequence. Since we
 * will need a list/hash without sequences.
 * We also want to avoid the memory of storing all sequences.
 * This will mean changing my constructor
 */

/* ---------------- structures and constants ----------------- */
static const char COMMENT = '>';
static const char *NOSTRING = "";
static const char GAP = '-';
static const unsigned N_SEQBUF = 100;

/* ---------------- fseq::fill  ------------------------------
 * This is the real initialisor/constructor for an fseq.
 * strangely, I used to use temporary variables for the strings,
 * but valgrind said there was a memory leak. It went away when
 * I swapped to working with the members of the class directly.
 */
bool
fseq::fill (ifstream &infile, const size_t len_exp) {
    string errmsg = __func__;
    cmmt = NOSTRING;
    seq  = NOSTRING;
    if (! getline(infile, cmmt))
        return false;   /* Not an error. Most likely end of file */
    if (cmmt[0] != COMMENT) {
        errmsg += + "no comment line starting "+ cmmt.substr(0,40);
        throw runtime_error (errmsg );
    }
    {
        streampos pos;
        string t;
        while ( getline (infile, t)) {
            if (t[0] == COMMENT) { /* Start of next sequence */
                infile.seekg(pos); /* Go back to before we tried reading */
                break;
            }
            seq += t;
            pos = infile.tellg();
        }
    }
    if (! seq.length() > 0) {
        cerr << "no sequence found for " << cmmt << "\n";
        return false;
    }
    if (len_exp) {
        if ( seq.length() != len_exp) {
            ostringstream convert;
            convert << __func__<< ": expected seq length: " << len_exp
                    << ", got " << seq.length()
                    << " for sequence starting\n" << cmmt << "\n";
            throw runtime_error (convert.str() );
        }
    }
    return true;
}

/* ---------------- fseq::fseq -------------------------------
 * Given an ifstream, try to return a sequence with comment
 * and sequence.
 * Sometimes (when reading an MSA) we have an idea how long the
 * sequence should be. If so, test for it. If len_exp == 0, then
 * we do not know what we are looking for, so do not worry.
 */
fseq::fseq(ifstream &infile, const size_t len_exp) {
    fill (infile, len_exp);
}

/* ---------------- fseq_prop::fseq_prop ---------------------
 * This constructor gets a sequence, does some work and sets
 * a few properties.
 */

fseq_prop::fseq_prop (fseq& fs)
{
    ngap = 0;
    sacred = false;
    string s = fs.get_seq();
    string::iterator it = s.begin();
    const string::iterator it_end = s.end();
    for (;it < it_end; it++)
        if (*it == GAP)
            ngap++;
}
