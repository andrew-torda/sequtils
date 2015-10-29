#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

/* ---------------- get_seq_list -----------------------------
 * I found that the string version of getline is not thread
 * safe. Presumably, it has in internal static buffer.
 */
static size_t
mgetline ( std::ifstream& is, std::string& str)
{
    size_t n = 0;
    for (int i = is.get();(i != EOF) && (i != '\n'); i = is.get()) {
        str += i;
        n++;
    }
    return n;
}


#ifdef want_test_of_mgetline
main()
{
    std::string s;
    std::ifstream infile ("test.txt");
    while (size_t i = mgetline (infile, s)) {
        s.erase();
        std::cout << s << "\n";
        std::cout << "got " << i << "chars\n";
    }
}
#endif /* want_test_of_mgetline */
