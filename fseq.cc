/* 10 oct 2015 */
static const char __attribute__((unused)) *rcsid = "$Id: fseq.cc,v 1.8 2015/12/17 12:19:49 torda Exp torda $";

#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <boost/regex.hpp> /* when we update gcc, get rid of this */

#include "fseq.hh"
#include "mgetline.hh"

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

/* ---------------- fseq::fill  ------------------------------
 * The real initialisor/constructor for an fseq.
 */
bool
fseq::fill (ifstream &infile, const size_t len_exp) {
    seq.clear();
    cmmt.clear();

    string errmsg = __func__;
    if (! mgetline(infile, cmmt))
        return false;   /* Not an error. Most likely end of file */
    if (cmmt[0] != COMMENT) {
        errmsg += + "no comment line starting "+ cmmt.substr(0,40);
        throw runtime_error (errmsg );
    }
    {
        streampos pos;
        for (string t; mgetline(infile, t); ) {
            if (t[0] == COMMENT) { /* Start of next sequence */
                infile.seekg(pos); /* Go back to before we tried reading */
                break;
            }
            seq += t;
            pos = infile.tellg();
        }
    }
    
    if (! (seq.length() > 0)) {
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

/* ---------------- fseq::clean ------------------------------
 * Remove white spaces from a sequence.
 * If keep_gap is not set, remove gap characters.
 */
void
fseq::clean (bool keep_gap)
{
    const boost::regex white("\\s");  /* Here is where we want */
    const boost::regex gap("-");      /* to move to std library */
    string replace = "";              /* when we update gcc */
    seq = boost::regex_replace (seq, white, replace);
    if (! keep_gap)
        seq = boost::regex_replace (seq, gap, replace);
}

/* ---------------- fseq::write ------------------------------
 * Given an open file, write the sequence, nicely formatted
 * to the file.
 */
void
fseq::write (ostream &ofile, const unsigned short line_len)
{
    string::size_type done = 0, to_go;
    ofile << this->get_cmmt() << '\n'; /* Write comment verbatim */
    string s = this->get_seq();  /* lines that should be split into pieces. */
    to_go = s.length();
    while (to_go) {
        string::size_type this_line = line_len;
        if (line_len > to_go)
            this_line = to_go;
        ofile << s.substr (done, this_line)<< '\n';
        done += this_line;
        to_go -= this_line;
    }
}
