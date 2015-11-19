/*
 * 19 Nov 2015
 */
#include <string>

#include "fseq_prop.hh"
#include "fseq.hh"
/* ---------------- fseq_prop::fseq_prop ---------------------
 * This constructor gets a sequence, does some work and sets
 * a few properties.
 */
static const char GAPCHAR = '-';
fseq_prop::fseq_prop (fseq& fs)
{
    ngap = 0;
    sacred = false;
    const std::string s = fs.get_seq();
    for ( std::string::const_iterator it = s.begin(); it < s.end(); it++)
        if (*it == GAPCHAR)
            ngap++;
}
