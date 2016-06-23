/* 3 jan 2016
 * Can only be included after <string> and <vector>
 */

#ifndef PATHPRINT_HH
#define PATHPRINT_HH
/* ---------------- path  ------------------------------------
 * Class for storing a path.
 * We want to be able to do nice printing. What we have to
 * store are things like the node index in final graph, distance
 * to source and preceding node. We will want to print out the
 * longest edge and the names of the sequences. I cannot see
 * any reason that we need anything more sophisticated than
 * a vector. The only important thing is that they be stored
 * in the right order. Let us put them in backwards. This makes
 * it more natural to go from source to destination.
 */

class dist_mat;
#ifdef __clang__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wpadded"
#endif /* clang */

class seq_index;
class path {
private:    
    struct node {
        unsigned label;      /* Index to node number in original set of seqs */
        unsigned pred_label; /* label of predecessor node in path */
        float src_dist;      /* Distance to source */
        float dst_dist;      /* Distance to dest (last) node */
        float p_dist;        /* Distance to predecessor */
        float orig_src_dist; /* Dist to source in original distance matrix */
        float orig_dst_dist; /* Dist to destination in original distance matrix */
    };
    std::vector<struct node> nodes;
    std::string nice_string (const node & node) const;
    unsigned n_mbr;
public:
    path (const std::vector<src_dist_t> &, const unsigned,
          const unsigned, const dist_mat &);
    int write_seqs (const char *seq_out_fname, const dist_mat &d_m, seq_index &s_i);
    int print (const char *outfile, const dist_mat &d_m) const ;
#   ifdef want_path_on_path
        void on_path (std::vector<bool> &v_on_path);
#   endif /* want_path_on_path */    
};

#ifdef __clang__
#    pragma GCC diagnostic pop
#endif /* clang */

#endif /* PATHPRINT_HH */
