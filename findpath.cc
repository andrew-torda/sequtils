/* 15 Dec 2015
 * The grand plan is to find the shortest path between two (or n)
 * sequences given a distance matrix. Why do we say n sequences ?
 * We may have two sequences. Then the problem is to find the path
 * between them.
 * We may have a few sequences and we want a list of the sequences that
 * are within the space
 */

#include <cstring> /* strerror */
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "distmat_rd.hh"
#include "prog_bug.hh"

using namespace std;

/* ---------------- structures and constants -----------------
 */
struct seq_dist {
    string s;
    unsigned i,j;
};

static const unsigned INVALID_NODE = (unsigned) -1;

/* ---------------- usage ------------------------------------
 */
static int
usage (const char *progname, const char *s)
{
    static const char *u
        = ": [-v] interesting_seq_file dist_mat_file\n";
    cerr << progname << ": "<<s << '\n' << progname << u;
    return EXIT_FAILURE;
}

/* ---------------- get_spec_seqs ----------------------------
 * This are the special sequences. For the moment, there are
 * just two, but we use a vector. In the future, we will use
 * this code for larger sets of sequences.
 */
static int
get_spec_seqs (const char *seq_fname, vector<string> &v_spec)
{
    string errmsg = __func__;
    ifstream infile (seq_fname);
    int nseq = 0;
    if (!infile) {
        errmsg += ": opening " + string (seq_fname) + ": " + strerror(errno) + '\n';
        cerr << errmsg;
        return (0);
    }
    string s;
    while (getline (infile, s)) {
        v_spec.push_back (s);
        nseq++;
    }
    infile.close();
    return nseq;
}

/* ---------------- get_special_seq_ndx ----------------------
 * We have the comments corresponding to some sequences in
 * one vector. Read down the list of sequences and return the
 * corresponding indices.
 */
static int
get_special_seq_ndx (const vector<string> &v_cmt,
                     const vector<string> &v_spec_seqs, vector<unsigned> &v_spec_ndx)
{
    vector<string>::const_iterator v_it_spec = v_spec_seqs.begin();
    for (; v_it_spec < v_spec_seqs.end(); v_it_spec++) {
        vector<string>::const_iterator v_it_cmt = v_cmt.begin();
        for (unsigned i = 0; v_it_cmt < v_cmt.end(); v_it_cmt++, i++){
            string s = *v_it_cmt;
            string t = *v_it_spec;
            if (s == t) {
                v_spec_ndx.push_back(i);
                break;
            }
        }
        if (v_it_cmt == v_cmt.end())
            cerr << __func__ << ": string not found: \""
                 << *v_it_spec << "\" in the dist matrix file\n";
    }
    if (v_spec_ndx.size() != v_spec_seqs.size())
        return EXIT_FAILURE;
    v_spec_ndx.shrink_to_fit();
    return EXIT_SUCCESS;
}

/* ---------------- component --------------------------------
 */

typedef struct {
    unsigned m1;
    unsigned m2;
    float dist;
} edge;
typedef vector<edge> cmpnt_edges;
class component {
private:
    cmpnt_edges edges;
    vector<int>::size_type n_node;
    vector<int>::size_type n_edge;
public:
    component(const cmpnt_edges &);
    cmpnt_edges get_edges() const { return edges;}
    vector<int>::size_type get_n_node() const { return n_node;}
    vector<int>::size_type get_n_edge() const { return n_edge;}
};

component::component(const cmpnt_edges &c_e)
{
    edges = c_e;
    n_node = 0;
    n_edge = 0;
    unordered_set<unsigned> set;
    vector<edge>::const_iterator v_it = edges.begin();
    for (; v_it < edges.end(); v_it++) {
        set.insert(v_it->m1);
        set.insert(v_it->m2);
    }
    n_node = set.size();
    n_edge = c_e.size();
}

/* ---------------- comp_contains ----------------------------
 * Does a component (graph) contain any of the nodes on our
 * list of magic nodes ? If so, remove that node from the list.
 */
static void
comp_contains (const cmpnt_edges &c, vector<unsigned> &v_to_find)
{
    vector<unsigned>::iterator v_it = v_to_find.begin();
    for (; v_it < v_to_find.end(); v_it++) {
        vector<edge>::const_iterator e_it = c.begin();
        for (; e_it < c.end(); e_it++) {
            if (*v_it == e_it->m1 || *v_it == e_it->m2) {
                    v_to_find.erase (v_it);
                    return ;
            }
        }
    }
}
/* ---------------- get_edges --------------------------------
 * We have a vector of special vertices, as their indices.
 * We read edges, collecting them into connected components.
 * When the first component contains the desired edges, we
 * return it.
 */
static component
get_edges (const vector<unsigned> &v_spec_ndx,
           const vector<struct dist_entry> &dist_mat_ent, const unsigned vbsty)
{
    vector<cmpnt_edges> all_graphs;
    /* Set up the array of connected components.
     * Put each magic sequence into its own component */
    {
        vector<unsigned>::const_iterator v_it = v_spec_ndx.begin();
        for (; v_it < v_spec_ndx.end(); v_it++) {
            cmpnt_edges c;
            edge e = {*v_it, *v_it, -1.0}; /* This is a fake edge that we will */
            c.push_back(e);          /* remove at end */
            all_graphs.push_back(c);
        }
    }

    /* We will check all the components in turn.
     * The edge has two nodes. It might appear in zero, one or two
     * components. We stop looking when we run out of distances or
     * if we have found all of the special sequences. The first special
     * sequence is in the first component by construction, so we do not
     * look for that. If we merge another component into the first, we
     * check if it contains one of the special sequences (other than
     * the first). We keep a copy of the list of nodes to look for
     * in v_to_find. It is not worth using a list or map since the set is
     * so small (typically only one member).
     */

    vector<unsigned> v_to_find = v_spec_ndx; /* set of special nodes */
    v_to_find.erase (v_to_find.begin());

    vector<struct dist_entry>::const_iterator d_it = dist_mat_ent.begin();
    for (; d_it < dist_mat_ent.end() && v_to_find.size() > 0; d_it++) {
        const unsigned first  = d_it->ndx1;
        const unsigned second = d_it->ndx2;
        const float dist      = d_it->dist;
        vector<cmpnt_edges>::size_type f1, f2;
        unsigned char nfound = 0; /* can only have values 0, 1 or 2 */
        vector<cmpnt_edges>::const_iterator c_it = all_graphs.begin();
        unsigned i_cmpnt[2];
        unsigned i = 0;
        for (f1 = 0, f2 = 0; c_it < all_graphs.end(); c_it++, i++) {
            vector<edge>::const_iterator e_it = c_it->begin();
            vector<edge>::const_iterator t = c_it->begin();
            if (vbsty > 2) {
                cout << "Graph "<< i << ": edges\n";
                for (; t< c_it->end();t++)
                    cout<< t->m1<< " "<< t->m2 <<'\n';
                cout << '\n';
            }
            for (; e_it < c_it->end(); e_it++) { /* within one component */
                if (first  == e_it->m1 || first  == e_it->m2 ||
                    second == e_it->m2 || second == e_it->m2) {
                    i_cmpnt[nfound] = i;
                    nfound++;
                    break;
                }
            }
        }
        switch (nfound) {
        case 0:
            {
                edge e = { first, second, dist};  //xxxxxxxx
                cmpnt_edges c;
                c.push_back(e);
                all_graphs.push_back(c);
                if (vbsty > 2)
                    cout <<__func__<< ": start new component with edge " <<first<< " "
                         << second<<'\n';
            }
            break;
        case 1: /* Add to existing component */
            {
                edge e = { first, second, dist};
                all_graphs[i_cmpnt[0]].push_back (e);
            }
            if (vbsty > 2)
                cout<< __func__ << ": adding "<< first << " "<< second
                    << " to component "<< i_cmpnt[0]<<'\n';
            break;
        case 2: /* Add the edge, then join the two components */
            {
                if (vbsty > 2)
                    cout << __func__<< ": moving component "<<i_cmpnt [1]
                         << " to "<< i_cmpnt[0]<< " because of "<< first<< " "<<second<<'\n';
                cmpnt_edges *c1 = &(all_graphs[i_cmpnt[0]]);
                edge e = { first, second, dist};
                c1->push_back(e);
                cmpnt_edges c2 = all_graphs[i_cmpnt[1]];
                c1->insert(c1->end(), c2.begin(), c2.end());
                all_graphs.erase(all_graphs.begin() + i_cmpnt[1]);
                if (i_cmpnt[0] == 0) /* are we merging into first component ? */
                    comp_contains (c2, v_to_find );
            }
            break;
        default:
            prog_bug (__FILE__, __LINE__, "Stupid");
        }

    }
    /* We are finished with merging and finding nodes. We have to copy the
     * first graph and then clean it up. If we have n special nodes, there
     * should be n edges we do not want.
     */
    cmpnt_edges final_edges;
    final_edges = all_graphs[0]; /* What we will return */
    for (unsigned i = 0; i < v_spec_ndx.size(); i++) {
        vector<edge>::iterator v_it = final_edges.begin();
        for (; v_it < final_edges.end(); v_it++) {
            if (v_it->m1 == v_it->m2) {
                final_edges.erase (v_it);
                break;
            }
        }
    }

    component c(final_edges);
    return c;
}

/* ---------------- describe_cmpnt ---------------------------
 */
static void
describe_cmpnt (const component &c, const dist_mat &d_m)
{
    vector<string>::size_type n = d_m.get_n_mem();
    cout << "The connected component has " << c.get_n_node()
         << " sequences and "<< c.get_n_edge()<< " edges.\n"
         << "The distance matrix had " << n << " sequences and "
         << (n*(n-1))/2 << " edges.\n";
}

typedef struct  {
    float dist; /* distance to source */
    unsigned pred;  /* Index of predecessor node */
    unsigned label; /* For relating back to names in original set */
    float p_dist; /* distance to predecessor */
} src_dist_t;

/* ---------------- main_graph -------------------------------
 */
typedef struct {
    unsigned ndx;
    float dist;
} nbor_dist_t;

class main_graph {
public:
    main_graph (const component &, const unsigned, const unsigned);
    void insert (const edge &, const unsigned src);
    void get_path ();
    unsigned get_next(const vector<bool>);
    unsigned src;     /* index of source   */
    unsigned dst;     /* and destination nodes */
    typedef vector<nbor_dist_t> n_list; /* Adjacency list */
    n_list & get_adjlist (const unsigned i)   { return adj_lists[i];}
    vector <src_dist_t> src_dist;
private:
    vector <n_list> adj_lists;
    unordered_map<unsigned,unsigned> label2ndx;
    void add_index ( const unsigned node_label, const unsigned ndx);
    void insert_helper (const unsigned, const unsigned, float dist, const unsigned);
};

void
main_graph::insert_helper ( const unsigned label1, const unsigned label2,
                            float dist, const unsigned src_label)
{
    unsigned ndx1 = label2ndx[label1];
    unsigned ndx2 = label2ndx[label2];
    n_list &n_l = get_adjlist(ndx1);      /* Push distance to node 2 */
    n_l.push_back( {ndx2, dist});         /* on to node 1's list */

    /* The node is now on somebody's list. Now check if we have
     * the source node. */
    if (label1 == src_label) {
        src_dist_t &t = src_dist [ndx2];
#       ifdef debug_till_we_puke        
            if (t.dist != numeric_limits<float>::max())
                prog_bug (__FILE__, __LINE__, "Hit node twice");
#       endif /* debug_till_we_puke */            
        if (dist == 0.0)
            dist += numeric_limits<float>::epsilon();
        t.dist = dist;
        t.label = label2;
    }
}

void
main_graph::insert (const edge &e, const unsigned src_label)
{
    float dist = e.dist;
    insert_helper (e.m1, e.m2, dist, src_label);
    insert_helper (e.m2, e.m1, dist, src_label);
}

/* ---------------- main_graph::add_index --------------------
 */
void
main_graph::add_index ( const unsigned node_label, const unsigned ndx) {
    src_dist_t tmp;
    tmp.dist = numeric_limits<float>::max();
    label2ndx[node_label] = ndx;
    tmp.pred = INVALID_NODE;
    tmp.label = node_label;
    tmp.p_dist = -1.0;      /* Junk, just so as to highlight any bugs */
    src_dist.push_back(tmp);
}

/* ---------------- main_graph constructor--------------------
 * This is built from a set of edges that is called a component.
 */
main_graph::main_graph (const component &cmpnt, const unsigned src_label, const unsigned dst_label)
{
    unordered_set<unsigned> set;
    {
        const vector<int>::size_type n = cmpnt.get_n_node();
        adj_lists.reserve (n);
        n_list empty_vec;
        adj_lists.insert (adj_lists.begin(), n, empty_vec);
        src_dist.reserve  (n);
        label2ndx.reserve (n);
    }
    /* Make our indexing from labels to ndx and simultaneously set
     * up the src_dist array.
     */
    {
        unsigned ndx = 0;
        for (const edge e: cmpnt.get_edges()) {
            auto not_yet = label2ndx.end();
            if (label2ndx.find(e.m1) == not_yet) {
                add_index (e.m1, ndx++);
                if (e.m1 == src_label)
                    src = label2ndx[e.m1];
                if (e.m1 == dst_label)
                    dst = label2ndx[e.m1];
                not_yet = label2ndx.end(); /* There could have been a rehash */
            }
            if (label2ndx.find(e.m2) == not_yet) {
                add_index (e.m2, ndx++);
                if (e.m2 == src_label)
                    src = label2ndx[e.m2];
                if (e.m2 == dst_label)
                    dst = label2ndx[e.m2];
            }
        }
    }
    src_dist[src].dist = 0.0; /* Set the source distance to itself */

    /* Now have the src_dist array with indices and all distances set to max_float */
    /* Next, insert the edges into the adjacency lists. */

    for (const edge e: cmpnt.get_edges())
        insert(e, src_label);

    for (nbor_dist_t n : get_adjlist (src)) {
        src_dist[n.ndx].dist = n.dist;
        src_dist[n.ndx].pred = src;
        src_dist[n.ndx].p_dist = n.dist;
    }

    for (n_list &n : adj_lists)
        n.shrink_to_fit();
}

/* ---------------- main_graph::get_next ---------------------
 * This is the part where one could use a priority queue.
 */
unsigned
main_graph::get_next (const vector<bool> known)
{
    float dist = numeric_limits<float>::max();
    unsigned ret = 0;
    for (unsigned i = 0; i < src_dist.size(); i++) {
        if (src_dist[i].dist < dist && !known[i]) {
            ret = i;
            dist = src_dist[i].dist;
        }
    }
    return ret;
}

/* ---------------- main_graph::get_path ---------------------
 * How did we get to the dst from source ?
 *
 */
void
main_graph::get_path ()
{
    unsigned node = dst;
    unsigned prev;
    cout << __func__ << " printing, source is "<< src << '\n';

    do {
        cout << "Node " << node << " labelled "<< src_dist[node].label
             << " dist to source "<< src_dist[node].dist
             << " dist to next node " << src_dist[node].p_dist << '\n';
        prev = node;
        node = src_dist[node].pred;
    } while (src_dist[prev].pred != INVALID_NODE);
}

/* ---------------- dijkstra ---------------------------------
 * The problem with our data is that we read up a big list of
 * distances. We could put them into a big matrix, but for n
 * original sequences, this means storing n(n-1)/2 entries.
 * Here is the memory cheap version.
 */
static void
dijkstra (const component &cmpnt, const dist_mat &d_m, const vector<unsigned> v_spec_ndx)
{
    if (v_spec_ndx.size() != 2)
        prog_bug (__FILE__, __LINE__, "Handle more than two nodes later");

    unsigned src_label = v_spec_ndx[0];
    unsigned dst_label = v_spec_ndx[1];
    class main_graph main_graph (cmpnt, src_label, dst_label);
    vector<bool> known (cmpnt.get_n_node(), false);

    while ( known[main_graph.dst] == false) {
        vector <src_dist_t>  &src_dist = main_graph.src_dist;
        unsigned current = main_graph.get_next (known);
        known [current] = true; /* here, set the current_distance */
        const float curr_dist = src_dist[current].dist;
        main_graph::n_list nl = main_graph.get_adjlist(current);
        for (nbor_dist_t n : nl) {
            float dist = n.dist;
            unsigned ndx = n.ndx;
            if ( !known[ndx]) {
                float trial = curr_dist + dist;
                if (src_dist[ndx].dist > trial) {
                    src_dist[ndx].dist   = trial;
                    src_dist[ndx].pred   = current;
                    src_dist[ndx].p_dist = dist;
                }
            }
        }
    }

    /* Now we have the path and distances. Have to think about how to return the
     * result */
    main_graph.get_path();
 }

/* ---------------- main  ------------------------------------
 */
int
main ( int argc, char *argv[])
{
    const char *progname = argv[0],
               *seq_fname = NULL,
               *mat_in_fname = NULL;
    unsigned vbsty = 1;
    seq_fname = argv[1];
    mat_in_fname = argv[2];

    if (argc < 3)
        return (usage(progname, "Too few arguments"));

    vector<string> v_spec_seqs;

    if (get_spec_seqs (seq_fname, v_spec_seqs) < 2) {
        cerr<< "Too few sequences found in "<< seq_fname <<'\n';
        return EXIT_FAILURE;
    }
    dist_mat d_m(mat_in_fname);
    vector<unsigned> v_spec_ndx;
    if (get_special_seq_ndx(d_m.get_cmt(), v_spec_seqs, v_spec_ndx) == EXIT_FAILURE)
        return EXIT_FAILURE;

    component cmpnt = get_edges (v_spec_ndx, d_m.get_dist(), vbsty);
    describe_cmpnt (cmpnt, d_m);
    dijkstra (cmpnt, d_m, v_spec_ndx);
    return EXIT_SUCCESS;
}
