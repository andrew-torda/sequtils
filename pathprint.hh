/* 3 jan 2016
 * Can only be included after <vector>
 */

#ifndef __PATHPRINT_HH
#define __PATHPRINT_HH
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
class path {
private:
    struct node {
        unsigned label;  /* Index to node number in original set of seqs */
        float src_dist;  /* Distance to source */
        float dst_dist;  /* Distance to dest (last) node */
        float p_dist;    /* Distance to predecessor */
    };
    std::vector<struct node> nodes;
public:
    path (const std::vector<src_dist_t> &, const unsigned, const unsigned, const dist_mat &);
    unsigned n_mbr;
};

#endif /* __PATHPRINT_HH */
