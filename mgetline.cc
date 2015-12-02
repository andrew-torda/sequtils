#include <fstream>
#include <string>

#include "mgetline.hh"

/* ---------------- get_seq_list -----------------------------
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
    static const unsigned BSIZ = 200;
    char buf[BSIZ];
    str.clear();
    bool blank = false;
    do {
        blank = false;
        is.getline (buf, BSIZ);  /* 99.9 % of the time, this is all we do. */
        str.append (buf);        /* In the very rare cases that the line is */
        if (is.eof())            /* too long for the buffer, we will enter */
            break;               /* this loop. */
        if (is.bad())
            break; /* this should not happen. */
        if (is.fail())
            is.clear();
        if (buf[0] == '\0')      /* Jump over blank lines */
            blank = true;

    } while (str.size() == (BSIZ - 1) || blank);
    
    return str.size();
}

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
