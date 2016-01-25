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

#ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wpadded"
#endif /* clang */

class dist_mat {
private:
    std::vector<dist_entry> v_dist;
    std::vector<std::string> v_cmt;
    bool fail_bit;
public:
    dist_mat (const char *);
    std::vector<std::string> get_cmt_vec() const {return v_cmt;}
    std::vector<dist_entry> get_dist() const {return v_dist;}
    std::vector<std::string>::size_type get_n_mem() const {return v_cmt.size();}
    std::string get_cmt(const unsigned i) const { return v_cmt[i];}
    float get_pair_dist (const unsigned, const unsigned) const ;
    bool operator!() const { return !fail_bit ;}
    bool fail() {return fail_bit;}
};
#ifdef __clang__
#    pragma clang diagnostic pop
#endif /* clang */

#endif /* DISTMAT_RD_HH */
