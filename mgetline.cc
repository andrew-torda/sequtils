#include <fstream>
#include <iostream> /* just for cerr during debugging*/
#include <string>

/* ---------------- get_seq_list -----------------------------
 * I found that the string version of getline is not thread
 * safe. Presumably, it has in internal static buffer.
 * Note that we do not have to append a terminating \0.
 * We always tell string() how many characters to copy.
 * This should mean it does not have to search the string for
 * a null.
 */
size_t
mgetline ( std::ifstream& is, std::string& str)
{
    static const unsigned BSIZ = 200;
    size_t ibuf = 0;
    size_t n    = 0;   /* the count that we return */
    char buf[BSIZ];
    str.clear();
    do {
        is.getline (buf, BSIZ);  /* 99.9 % of the time, this is all we do. */
        str.append (buf);        /* In the very rare cases that the line is */
        if (is.eof())            /* too long for the buffer, we will enter */
            break;               /* this loop. */
        if (is.bad())
            break; /* this should not happen. */
        if (is.fail())
            is.clear();
    } while (str.size() == (BSIZ - 1));
    
#   ifdef old
    int c = is.get();
    while ((c != EOF) && (c != '\n')) {
        n++;
        buf[ibuf] = c;
        if (++ibuf == BSIZ) {
            ibuf = 0;
            str.append (buf, BSIZ);
        }
        c = is.get();
    }
    if (ibuf)
        str.append (buf, ibuf);
    return n;
#   endif /* old */
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
