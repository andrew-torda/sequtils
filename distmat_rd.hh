/*
 * 22 oct 2015
 */
struct dist_entry {
    float dist;
    unsigned ndx1;
    unsigned ndx2;
};

int read_distmat (const char *, std::vector<dist_entry> &, std::vector<std::string> &);
