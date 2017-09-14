/* Nov 2015
 * If I use gcc 4.8, there was a data race within the call to getline().
 * I could avoid races by putting a mutex around the code, but this went
 * rather slowly.
 * With clang, there is no race condition, so I do not need the mutex.
 * At the same time, I do not want to add more #ifdefs and make the
 * code harder to read.
 * I wrote line_by_bytes to call read() on each character. There are no
 * race conditions and no need for mutexes, but it does seem a bit slow.
 * To really improve it, I would call read() with a bit buffer.
 */

#include <cstring>
#include <fstream>
#include <iostream> /* only during debugging */
#include <limits>
#include <string>

#include "bust.hh"
#include "mgetline.hh"
#include "prog_bug.hh"

/* Currently, there is a race condition in the library version of
 * getline().
 */

/* ---------------- statics and globals ----------------------
 */
static const char LINE_FINISHED = 0;  /* Return values from */
static const char MORE_COMING   = 1;  /* line_by_bytes() */
#ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wglobal-constructors"
#endif /* clang */

#ifdef __clang__
#    pragma clang diagnostic pop
#endif /* clang */


/* ---------------- line_by_bytes ----------------------------
 * This is another variation, trying to avoid the problems of
 * contention within libstdc's getline(). We can avoid the
 * slowness of the mutex, but this does very slow input,
 * character by character from the file pointer.
 * What inspired me to use is.readstate() ? This function
 * should return a value that says the line is not finished.
 */
static char
line_by_bytes (std::ifstream &is, char *buf, const unsigned BSIZ)
{
    unsigned got = 0;
    char c = '\0';
    char retval = LINE_FINISHED;
    do {
        is.read (&c, 1);
        buf[got++] = c;
    } while ((got < BSIZ - 1) && (! is.eof()) && (c != '\n'));

    if (is.eof()) {
        buf[ got - 1] = '\0';
    } else if (c == '\n') {
        buf [ got -1] = '\0'; /* this overwrites the newline */
    } else {  /* our buffer was too small */
        buf[got] = '\0';
        retval = MORE_COMING;
    }
    
    return retval;
}

/* ---------------- mgetline ---------------------------------
 * This version of getline throws away anything after a comment
 * character
 * Usually, we will only have to read one line, but if necessary
 * we go in the do loop and read on.
 */
unsigned
mc_getline ( std::ifstream& is, std::string& str, const char cmmt)
{
    static const unsigned BSIZ = 256;
    char buf[BSIZ];
    str.clear();
    bool blank = false;
    bool do_comment = false;
    bool line_finished;
    bool continuing = false;
    if (cmmt != '\0')
        do_comment = true;
    do {
        line_finished = false;
        is.clear();
        blank = false;
#       ifdef use_slow_locks
            mtx.lock();
            is.getline (buf, BSIZ);
            mtx.unlock();
#       else    /* use the slow character reading, but no locks */        
            if (line_by_bytes(is, buf, BSIZ) == LINE_FINISHED)
                line_finished = true;
            else
                continuing = true;
#       endif /* use_slow_locks */    
        if (do_comment) {
            const size_t len = strlen (buf);
            const char *end = buf + len;
            for (char *b = buf; b < end; b++)
                if (*b == cmmt) {
                    *b = '\0'; break; }
        }
        str.append (buf);
        if (line_finished && continuing)
            break;
        if (is.eof())
            break;
        else if (is.bad())
            break; /* this should not happen. */
        else if (buf[0] == '\0')      /* Jump over blank lines */
            blank = true;
    } while ( blank || line_finished == false);

    if (str.size() > std::numeric_limits<unsigned>::max())
        prog_bug (__FILE__, __LINE__, "Line too long");

    return (unsigned(str.size()));
}

/* ---------------- mgetline ---------------------------------
 * I found that the string version of getline is not thread
 * safe. It could be some static buffer, but I did not see
 * one. It might be a variable lurking within the library
 * code that checks for character widening.
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

/* ---------------- getline_delim ----------------------------
 * Read from file until we see the delimiting character, c_delim.
 * Return the string, without the delimiter.
 * If strip is true, remove white space. This uses a second buffer.
 * If eat_delim is true, then discard the delimiter. This is probably
 * what one wants for a newline. If, however, you are reading up to a
 * delimiter like ">", then you probably want to keep the delimiter for
 * the next read.
 */
void
getline_delim (std::ifstream &is, std::string &s, const bool eat_delim,
               const char c_delim, const bool strip)
{
    static const unsigned bsiz = 1024;
    s.clear();
    if (is.eof())
        return;

    do {
        char buf[bsiz];
        std::streamsize ngot = 0;
        bool delim_found = false;
        is.read(buf, bsiz);
        std::streamsize n_inbuf;
        if (is.gcount())
            n_inbuf = is.gcount();
        else                         /* we did not get any characters */
            return;
        char *t;
        if (! strip) {
            for ( t = buf; ngot < n_inbuf; t++, ngot++) {
                if (*t == c_delim) {
                    delim_found = true;
                    break;
                }
            }
            s.append (buf, size_t (ngot));
        } else {                                 /* remove white spaces */
            char nowhite[bsiz];
            char *p = nowhite;
            unsigned to_copy = 0;
            t = buf;

            for ( t = buf; (ngot < n_inbuf) ; t++, ngot++) {
                if (*t == c_delim) {
                    delim_found = true;
                    break;
                }
                if ( ! isspace (*t)) {
                    *p++ = *t;
                    to_copy++;
                }
            }
            s.append(nowhite, to_copy);
        }

        if (delim_found)  {
            std::streamoff back = ngot - n_inbuf;
            if (eat_delim)
                back++;

            is.clear();
            is.seekg (back, is.cur);
            return;
        }
        if (is.eof())
            return;
    } while (1);
}


#ifdef test_getl_jmp
#include <cerrno>
#include <iostream>
int
main (int argc, char *argv[])
{
    const char *in_fname = argv[1];
    cout << "opening "<< in_fname << endl;

    for (unsigned i = 1; i < 500; i += 100) {
        ifstream infile (in_fname);
        if (!infile)
            return (bust (__func__, "opening", in_fname, ":", strerror(errno), 0));
        cout << "Working with bufsiz = "<< i << '\n';
        string s;
        do {
            bool strip;
            getline_delim (infile, s, '\n', strip = false);
            cout << "Got \""<<s<< "\"\n"; s.clear();
            getline_delim (infile, s, '>', strip = true);
            cout << "Seq: \""<<s<< "\"\n";
        } while (s.length() && !infile.eof());
    }
}

#endif /* test_getl_jump */

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
