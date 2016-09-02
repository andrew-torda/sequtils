/*
 * 29 oct 2015
 * Can only be included after <fstream> and <string>.
 */
#ifndef MGETLINE_HH
#define MGETLINE_HH

unsigned mgetline ( std::ifstream& is, std::string& str);
unsigned mc_getline ( std::ifstream& is, std::string& str, const char cmmt);
void getline_delim (std::ifstream &is, std::string &s, const bool eat_delim,
                    const char c_delim, const bool strip);

#endif /* MGETLINE_HH */
