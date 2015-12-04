/*
 * 29 oct 2015
 * Can only be included after <fstream> and <string>.
 */
#ifndef MGETLINE_HH
#define MGETLINE_HH

unsigned mgetline ( std::ifstream& is, std::string& str);
unsigned mc_getline ( std::ifstream& is, std::string& str, const char cmmt);
#endif /* MGETLINE_HH */
