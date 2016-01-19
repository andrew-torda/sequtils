/*
 * 7 Jan 2016
 * This will end up as a function for another program.
 * The mission is to read a file of sequences.
 * For each sequence, get an index into the file (tellg) for each sequence.
 */

#include <stdexcept>
#include <fstream>
#include <iostream>
#include <istream>
#include <string>
#include <unordered_map>

#include "bust.hh"
#include "fseq.hh"
#include "mgetline.hh"
#include "seq_index.hh"
using namespace std;


#ifdef want_get_seq_by_num
/* ---------------- get_seq_by_num ---------------------------
 * Given an index of a sequence, return just that sequence.
 */
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
 */
fseq
seq_index::get_seq_by_cmmt (const string &s)
{
    seq_map::const_iterator it = s_map.find (s);
    if (it == s_map.end())
        cerr<< __func__<< " sequence not found:\n" << s<< '\n';
    infile.clear();
    infile.seekg (it->second);
    if (infile.bad() || infile.fail()) {
         string t = "Fail seeking on "; throw runtime_error (t + fname);}
    fseq fs;
    if (!fs.fill (infile, 0)) {
        string t = "Fail reading seq from "; throw runtime_error (t + fname);}
    return fs;

}

/* ---------------- get_one  ---------------------------------
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
 */
int
seq_index::fill (const char *fn)
{
    fname = fn;
    infile.open (fname);
    if (!infile)
        throw runtime_error (string("Open fail on ") + fname);
    
    line_read lr;
    while (get_one(&lr))
        if (lr.first[0] == '>')
            s_map.insert (lr);
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
