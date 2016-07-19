/*
 * 7 Jan 2016
 * This will end up as a function for another program.
 * The mission is to read a file of sequences.
 * For each sequence, get an index into the file (tellg) for each sequence.
 */

#include <cstring>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <istream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bust.hh"
#include "filt_string.hh"
#include "fseq.hh"
#include "mgetline.hh"
#include "seq_index.hh"
using namespace std;


/* ---------------- seq_index copy constructor ---------------
 * gcc 4.8 needs this. clang does not.
 */
seq_index::seq_index (const seq_index &s_in) {
    s_map   = s_in.s_map;
    indices = s_in.indices;
    fname   = s_in.fname;
    infile.open (s_in.fname);
    if (!infile)
        throw runtime_error (string (__func__) + ": open fail on " + fname + ": " +  strerror(errno));
}
seq_index& seq_index::operator= (seq_index &&s_in ) {
    s_map   = move(s_in.s_map);
    indices = move(s_in.indices);
    fname   = move(s_in.fname);
    infile.open (fname);
    if (!infile)
        throw runtime_error (string (__func__) + ": open fail on " + fname + ": " + strerror(errno));
    return *this;
}

/* ---------------- get_seq_by_num ---------------------------
 * Given an index of a sequence, return just that sequence.
 */
#ifdef want_get_seq_by_num
fseq
seq_index::get_seq_by_num (unsigned ndx)
{
    infile.clear();
    infile.seekg (indices[ndx]);
    if (infile.bad() || infile.fail()) {
        string s = "Fail seeking on "; throw runtime_error (s + fname);}
    fseq fs;
    if (!fs.fill (infile, 0)) {
        string s = "Fail reading seq from "; throw runtime_error (s + fname);}
    return fs;
}
#endif /* want_get_seq_by_num */

/* ---------------- get_seq_by_cmmt --------------------------
 * Given the comment / name for a sequence, look up its
 * position in the file and return it.
 * We store the string after removing leading and trailing
 * white space, so we have to copy the string.
 */
fseq
seq_index::get_seq_by_cmmt (const string &s)
{
    string t = s;
    seq_map::const_iterator it = s_map.find (rmv_white_start_end(t));
    const string seek_fail = "Fail seeking on file ";
    const string seq_not_found = "Sequence not found in " + fname + ":\n";
    const string fail_reading  = "Fail reading sequence from: ";
    if (it == s_map.end())
        throw runtime_error (seq_not_found + s + '\n');
    infile.clear();
    infile.seekg (it->second);
    if (infile.bad() || infile.fail())
        throw runtime_error (seek_fail + fname);
    fseq fs;
    if (!fs.fill (infile, 0))
        throw runtime_error (fail_reading + fname);
    return fs;

}

/* ---------------- get_one  ---------------------------------
 * Read a line from the current input file. Return a pair in
 * which the first element is our string. The second element
 * is the streampos where the string starts.
 */
unsigned
seq_index::get_one (line_read *lr)
{
    string s;
    streampos pos = infile.tellg();
    unsigned i = mgetline (infile, s);
    if (i) {
        lr->first = s;
        lr->second = pos;
    }
    return i;
}


/* ---------------- fill  ------------------------------------
 * This should be rewritten to use the getline with delimiter
 */
int
seq_index::fill (const char *fn)
{
    infile.open (fn);
    if (!infile) {
        cerr << __func__<< "Open fail on " << fn << ": "<< strerror (errno);
        return EXIT_FAILURE;
    }
    fname = fn;
    line_read lr;
    while (get_one(&lr)) {
        if (lr.first[0] == '>') {
            rmv_white_start_end (lr.first);
            indices.push_back(lr.second);
            s_map.insert (lr);
        }
    }
    indices.shrink_to_fit();
    return EXIT_SUCCESS;
}
    
/* ---------------- main   -----------------------------------
 */

#ifdef test_main
int
main ()
{
    seq_index s_i ( "/home/torda/c/seq_work/example/big.fa");

    seq_index::seq_map::const_iterator l = s_i.begin();
    for (unsigned i = 0; i < 3; i++) {
        fseq fs = s_i.get_seq_by_cmmt(l++->first);
    }
    const size_t n = s_i.size();
    l = s_i.begin();
    for (unsigned i = 0; i < (n-3); i++)
        l++;
    for (unsigned i = 0; i < 3; i++) {
        fseq fs = s_i.get_seq_by_cmmt((l++)->first);
        fs.clean(false);
        if (fs.get_seq() == "")
            break;
        fs.write(cout, 100);
    }
    cout<< "Hello\n";
}
#endif /* test_main */
