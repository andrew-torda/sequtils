/* Nov 2015
 */

#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <string>

#include "mgetline.hh"
#include "prog_bug.hh"

/* Currently, there is a race condition in the library version of
 * getline(). This is a bit ugly, but we put a lock around the one
 * offending line.
 */

static  std::mutex mtx;
/* ---------------- mgetline ---------------------------------
 * This version of getline throws away anything after a comment
 * character
 */
unsigned
mc_getline ( std::ifstream& is, std::string& str, const char cmmt)
{
    static const unsigned BSIZ = 200;
    char buf[BSIZ];
    str.clear();
    bool blank = false;
    bool do_comment = false;
    if (cmmt != '\0')
        do_comment = true;
    do {
        is.clear();
        blank = false;
        mtx.lock();
        is.getline (buf, BSIZ);  /* 99.9 % of the time, this is all we do. */
        mtx.unlock();
        if (do_comment) {
            const size_t len = strlen (buf);
            const char *end = buf + len;
            for (char *b = buf; b < end; b++)
                if (*b == cmmt) {
                    *b = '\0'; break; }
        }
        str.append (buf);        /* In the very rare cases that the line is */
        if (is.eof())            /* too long for the buffer, we will enter */
            break;               /* this loop. */
        if (is.bad())
            break; /* this should not happen. */
        if (buf[0] == '\0')      /* Jump over blank lines */
            blank = true;
    } while ( is.fail() || blank);

    if (str.size() > std::numeric_limits<unsigned>::max())
        prog_bug (__FILE__, __LINE__, "Line too long");

    return (unsigned(str.size()));
}

/* ---------------- mgetline ---------------------------------
 * I found that the string version of getline is not thread
 * safe. Presumably, it has in internal static buffer.
 * Note that we do not have to append a terminating \0.
 * We always tell string() how many characters to copy.
 * This should mean it does not have to search the string for
 * a null.
 *
 * This version jumps over blank lines.
 */
unsigned
mgetline ( std::ifstream& is, std::string& str)
{
    return mc_getline (is, str, '\0');
}


/* ---------------- testing  ---------------------------------
 */
#ifdef want_test_of_mgetline
#include <iostream>
main ()
{
    std::string s;
    std::ifstream infile ("test.txt");
    while (size_t i = mgetline (infile, s)) {
        std::cout << s << "\n";
        std::cout << "got " << i << "chars\n";
    }
}
#endif /* want_test_of_mgetline */
