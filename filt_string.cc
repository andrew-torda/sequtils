/*
 * 17 Nov 2015
 * We want to
 *  1. set up a bit vector for all positions in a string which are not gaps and
 *  2. use this filter to remove all the positions which are not set.
 */

#include <cerrno>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "filt_string.hh"
#include "fseq.hh"

static const char GAPCHAR = '-';
using namespace std;


/* ---------------- set_vec  ---------------------------------
 * Given a vector and a string, walk down the string. If a
 * character is not a gap, set the entry in the vector.
 * This function does not currently check if the vectors are the
 * same size. Perhaps it should and then throw an exception if
 * they differ.
 */
void
set_vec (const std::string &s, vector<bool> &v)
{
    string::const_iterator s_it = s.begin();
    vector<bool>::iterator v_it = v.begin();
    for ( ; s_it != s.end(); s_it++, v_it++)
        if (*s_it != GAPCHAR)
            if (! *v_it)
                *v_it = true;
}

/* ---------------- squash_string_vec ------------------------
 * Given a string and a vector of bools, walk down the string
 * and copy those positions where the boolean is true.
 */
std::string
squash_string_vec (std::string &s, const vector<bool> &v)
{
    class set_vec_except e;
    if (s.size() != v.size()) {
        e.s_siz = s.size();
        e.v_siz = v.size();
        throw e;
    }

    if (v.size() != s.size()) {
        std::cerr << __func__<< ": programming mistake. String and vector sizes are different. "
                  << s.size() << " != "<<v.size()<< "\n";
        return string ("");
    }
    string new_s;
    new_s.reserve(v.size());
    std::vector<bool>::const_iterator v_it = v.begin();

    for( std::string::iterator s_it=s.begin(); s_it!=s.end(); ++s_it, ++v_it)
        if (*v_it)
            new_s += *s_it;
    s = new_s;
    return s;
}

#undef test_me
#ifdef  test_me

#include <cstdlib>
int main ()
{
    const char *in_fname = "/home/torda/junk/y.fa";
    ifstream infile (in_fname);
    size_t len;
    if (! infile) {
        string errmsg = __func__;
        errmsg +=         errmsg += ": opening " + string (in_fname) + ": " + strerror(errno) + "\n";
        cerr << errmsg;
        return EXIT_FAILURE;
    } 
    { /* look at the first sequence, get the length, so we can check on */
        streampos pos = infile.tellg();             /* subsequent reads */
        fseq fs(infile, 0);
        len = fs.get_seq().size();
        infile.seekg (pos);
    }
    infile.seekg(0);
    vector<bool> v(len, false);
    cout<< __func__ << " len is "<< len << " and sizeof vector is "<< sizeof(v) << "\n";
    for (fseq fs(infile, len); fs.get_seq().size(); fs.fill(infile, len))
        set_vec (fs.get_seq(), v);

    v.shrink_to_fit();

    for (vector<bool>::const_iterator t = v.begin(); t != v.end(); t++)
        cout << *t;    
    cout << '\n';

    infile.close();
    infile.open (in_fname);
    for (fseq fs(infile, len); fs.get_seq().size(); fs.fill(infile, len)) {
        cout<< __func__<< ": before squash\n" <<
            fs.get_seq()<< '\n';
        string s = fs.get_seq();
        try {
            fs.replace_seq (squash_string_vec (s, v));
        } catch ( set_vec_except &e) {
            cerr << "BOOMsize of sequence \n" << e.what() << "\nStopping";
            return EXIT_FAILURE;
        }
                
        
        cout << __func__<< ": after\n" <<
            fs.get_seq() << "\n";
    }

}
#endif /* test_me */
