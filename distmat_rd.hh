/*
 * 22 oct 2015
 * Can only be included after <string> and <vector>
 */
#ifndef DISTMAT_RD_HH
#define DISTMAT_RD_HH
struct dist_entry {
    float dist;
    unsigned ndx1;
    unsigned ndx2;
};

int read_distmat (const char *, std::vector<dist_entry> &, std::vector<std::string> &);

class dist_mat {
private:
    std::vector<dist_entry> v_dist;
    std::vector<std::string> v_cmt;
public:
    dist_mat (const char *);
    const std::vector<std::string> get_cmt() const {return v_cmt;}
    const std::vector<dist_entry> get_dist() const {return v_dist;}
    std::vector<std::string>::size_type get_n_mem() const {return v_cmt.size();}
};
#endif /* DISTMAT_RD_HH */
