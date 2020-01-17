/* 3 Jan 2016
 * This is needed by both the main functions that do Dijstra and the functions
 * that collect the optimal path.
 */
#ifndef GRAPHMISC_H
#define GRAPHMISC_H

const unsigned INVALID_NODE = unsigned (-1);

typedef struct  {
    float dist;          /* distance to source */
    unsigned pred;       /* Index of predecessor node */
    unsigned label;      /* For relating back to names in original set */
    float p_dist;        /* distance to predecessor */
} src_dist_t;

#endif /* GRAPHMISC_H */
