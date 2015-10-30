#include <fstream>
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
#   ifdef slow_version
        for (int i = is.get();(i != EOF) && (i != '\n'); i = is.get()) {
            str += i;
            n++;
        }
#   endif /* slow_version */
    return n;
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
