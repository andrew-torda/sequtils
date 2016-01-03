/* 1 jan 2015
 * Print out a nice report about the path between two nodes.
 * We are going to need the list of nodes and the list of sequence
 * comments.
 * If we are given a file name, open it and print to it, otherwise
 * write to standard output.
 */

#include "path_report.hh"

/* move this into a shared file */
typedef struct  {
    float dist; /* distance to source */
    unsigned pred;  /* Index of predecessor node */
    unsigned label; /* For relating back to names in original set */
    float p_dist; /* distance to predecessor */
} src_dist_t;


/* ---------------- path_report   ---------------------------
 * Need, dst, src_dist, INVALID_NODE
 */
int
path_report (const unsigned src, const unsigned dst, vector<src_dist_t> src_
