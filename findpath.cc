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
#include <utility> /* used by seq_index */
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <unistd.h>
#include <vector>

#include "bust.hh"
#include "distmat_rd.hh"
#include "filt_string.hh"
#include "fseq.hh"
#include "graphmisc.hh"
#include "pathprint.hh"
#include "prog_bug.hh"
#include "seq_index.hh"

using namespace std;

/* ---------------- structures and constants -----------------
 */
struct seq_dist {
    string s;
    unsigned i,j;
};

static const float BAD_DIST = -1.0;

/* ---------------- usage ------------------------------------
 */
static int
usage (const char *progname, const char *s)
{
    static const char *u
        = "[-p seqs_on_path_fname] [-s seq_out_fname]\n\
           [-u unloved_seqs] interesting_seq_file dist_mat_file [seq_in_fname]\n";
    cerr << progname << ": "<< s<<'\n';
    return (bust (progname, u, NULL));
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
    if (!infile) {
        errmsg += ": opening file \"" + string (seq_fname) + "\": "
            + strerror(errno) + '\n';
        cerr << errmsg;
        return (0);
    }
    string s;
    int nseq = 0;
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
get_special_seq_ndx (const vector<string> &v_cmt_vec,
                     const vector<string> &v_spec_seqs, vector<unsigned> &v_spec_ndx)
{
    for (string t : v_spec_seqs) {
        rmv_white_start_end (t);
        vector<string>::const_iterator v_it_cmt = v_cmt_vec.begin();
        for (unsigned i = 0; v_it_cmt < v_cmt_vec.end(); v_it_cmt++, i++) {
            string s = *v_it_cmt;
            rmv_white_start_end (s);
            if (s == t) {
                v_spec_ndx.push_back(i);
                break;
            }
        }
        if (v_it_cmt == v_cmt_vec.end()) {
            string e = "seq not found: \"" + t + "\" in the dist matrix file\n";
            return (bust(__func__, e.c_str(), 0));
        }
    }
    if (v_spec_ndx.size() != v_spec_seqs.size())
        return (bust (__func__, "v_spec_ndx and v_spec_seqs, different sizes", 0));
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
    typedef vector<edge>::const_iterator const_it;
public:
    component(const cmpnt_edges &);
    cmpnt_edges get_edges() const { return edges;}
    const_it begin() const {return edges.begin();}
    const_it end()   const {return edges.end();}
    void push_back (const edge &e) {edges.push_back(e);}
    void insert (component &c2)
    {
        const vector<edge> e = c2.get_edges();
        edges.insert (edges.end(), e.begin(), e.end()) ;
    }

    bool has_node (const unsigned);
    void remove_dups ();
    void describe (const dist_mat &d_m);

    vector<int>::size_type get_n_node() const { return n_node;}
    vector<int>::size_type get_n_edge() const { return n_edge;}
};

component::component(const cmpnt_edges &c_e)
{
    edges = c_e;
    n_node = 0;
    n_edge = 0;
    unordered_set<unsigned> set;
    for (edge e: edges) {
        set.insert(e.m1);
        set.insert(e.m2);
    }
    n_node = set.size();
    n_edge = c_e.size();
}

/* ---------------- has_node ---------------------------------
 * return true / false, depending on whether this component
 * has a node with this number (ndx).
 * I will replace this with an unordered_set later.
 */
bool
component::has_node(const unsigned ndx)
{
    for (const edge e : edges)
        if (e.m1 == ndx || e.m2 == ndx)
            return true;
    return false;
}

/* ---------------- remove_dups ------------------------------
 * At the start, we put in edges of the form,
 * node1 node1 -1
 * Remove them.
 */
void
component::remove_dups ()
{
    vector<edge>::iterator e_it = edges.begin();
    for ( ; e_it != edges.end(); e_it++) {
        if (e_it->m1 == e_it->m2) {
            edges.erase(e_it);
            return;
        }
    }
}

/* ---------------- which_component --------------------------
 * Given the node number, find the edge it is in.
 */
static unsigned
which_component (const vector<component> &all_graphs, const unsigned node_ndx)
{
    vector<component>::const_iterator c_it = all_graphs.begin();
    for (; c_it != all_graphs.end(); c_it++) {
        for (edge e: *c_it)     /* Look at edges within this one component */
            if (node_ndx == e.m1 || node_ndx == e.m2)
                return (unsigned(c_it - all_graphs.begin()));
    }
    prog_bug (__FILE__, __LINE__, "Broke looking for node");
}

/* ---------------- get_edges --------------------------------
 * We have a vector of special vertices, as their indices.
 * We read edges, collecting them into connected components.
 * When the first component contains the desired edges, we
 * return it.
 */
static component
get_edges (const vector<unsigned> &v_spec_ndx,
           const vector<struct dist_entry> &dist_mat_ent)
{
    vector<component> all_graphs;
    /* We keep a list of nodes we have seen. If a node has not been seen
     * before, we void looking for it.
     */
    unordered_set<unsigned> seen_nodes;
    /* Set up the array of connected components.
     * Put each magic sequence into its own component */
    {
        for (unsigned v: v_spec_ndx) {
            cmpnt_edges c;
            edge e = {v, v, BAD_DIST}; /* This is a fake edge that we will */
            c.push_back(e);          /* remove at end */
            all_graphs.push_back(c);
            seen_nodes.insert(v);
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
        unsigned char nfound = 0; /* can only have values 0, 1 or 2 */
        unsigned c_ndx_1 = 0, c_ndx_2 = 0;
        bool first_known, second_known;
        const unordered_set<unsigned>::const_iterator e_not_found = seen_nodes.end();
        if (seen_nodes.find(first) == e_not_found)
             first_known = false;
        else first_known = true;
        if (seen_nodes.find(second) == e_not_found)
            second_known = false;
        else second_known = true;

        if ((!first_known) && (!second_known)) {
            nfound = 0;
        } else if ( first_known && second_known) {
            nfound = 2;
            c_ndx_1 = which_component (all_graphs, first);
            c_ndx_2 = which_component (all_graphs, second);
            if (c_ndx_1 > c_ndx_2)
                swap (c_ndx_1, c_ndx_2);
        } else {
            nfound = 1;
            if (first_known)
                c_ndx_1 = which_component (all_graphs, first);
            else
                c_ndx_1 = which_component (all_graphs, second);
        }

        switch (nfound) {
        case 0:  /* Start a new component */
            {
                edge e = { first, second, dist};
                cmpnt_edges c;
                c.push_back(e);
                all_graphs.push_back(c);
            }
            break;
        case 1: /* Add to existing component */
            {
                edge e = { first, second, dist};
                all_graphs[c_ndx_1].push_back (e);
            }
            break;
        case 2: /* If the edge joins two components, add it and merge them */
            {   /* If both edges are within one edge, forget it. */
                if (c_ndx_1 != c_ndx_2) {
                    component *c1 = &(all_graphs[c_ndx_1]);
                    edge e = { first, second, dist};
                    c1->push_back(e);
                    component c2 = all_graphs[c_ndx_2];
                    c1->insert(c2);
                    all_graphs.erase(all_graphs.begin() + c_ndx_2);
                    if (c_ndx_1 == 0)   /* Check if this is one of the special nodes we are waiting on */
                        for (vector<unsigned>::iterator v_it = v_to_find.begin(); v_it < v_to_find.end(); v_it++)
                            if (c2.has_node (*v_it))
                                v_to_find.erase (v_it);
                }
            }
            break;
        default:
            string s = "Bad bug. nfound is " + to_string (nfound);
            prog_bug (__FILE__, __LINE__, s.c_str());
        }
        seen_nodes.insert(first);
        seen_nodes.insert(second);
    }
    /* We are finished with merging and finding nodes. We have to copy the
     * first graph and then clean it up. If we have n special nodes, there
     * should be n edges we do not want.
     */

    for (unsigned i = 0; i < v_spec_ndx.size(); i++)
        all_graphs[0].remove_dups();

    component final_graph (all_graphs[0].get_edges());
    return final_graph;
}

/* ---------------- describe_cmpnt ---------------------------
 */
void
component::describe (const dist_mat &d_m)
{
    vector<string>::size_type n = d_m.get_n_mem();
    cout << "The connected component has " << this->get_n_node()
         << " sequences and "<< this->get_n_edge()<< " edges.\n"
         << "The original distance matrix had " << n << " sequences and "
         << (n*(n-1))/2 << " edges.\n";
}

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

/* ---------------- main_graph::insert_helper ----------------
 * This teeny little function takes a lot of the run time, presumably,
 * because it is called so often. Can we run a line profiler on it and
 * see what is taking the time ?
 */
void
main_graph::insert_helper ( const unsigned label1, const unsigned label2,
                            float dist, const unsigned src_label)
{
    unsigned ndx1 = label2ndx[label1];
    unsigned ndx2 = label2ndx[label2];
    n_list &n_l = get_adjlist(ndx1);      /* Push distance to node 2 */
    n_l.push_back( {ndx2, dist});         /* on to node 1's list */

    if (label1 == src_label) {         /* The node is on somebody's list. */
        src_dist_t &t = src_dist [ndx2];      /* Now check if we have the */
        if (dist == 0.0)                                  /* source node. */
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
    tmp.p_dist = BAD_DIST;      /* Junk, just so as to highlight any bugs */
    src_dist.push_back(tmp);
}

/* ---------------- main_graph constructor--------------------
 * This is built from a set of edges that is called a component.
 */
main_graph::main_graph (const component &cmpnt, const unsigned src_label, const unsigned dst_label)
{
    unordered_set<unsigned> set;
    const unsigned rubbish = numeric_limits<unsigned>::max();
    src = rubbish;
    dst = rubbish;
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
    if (src == rubbish)
        prog_bug (__FILE__, __LINE__, "src not found");
    if (dst == rubbish)
        prog_bug (__FILE__, __LINE__, "dst not found");
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

/* ---------------- dijkstra ---------------------------------
 * The problem with our data is that we read up a big list of
 * distances. We could put them into a big matrix, but for n
 * original sequences, this means storing n(n-1)/2 entries.
 * Here is the memory cheap version.
 */
static class path
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
    const path path (main_graph.src_dist, main_graph.src, main_graph.dst, d_m);
    return path;
}


/* ---------------- write_setof_seqs -------------------------
 * Go to the output file and write the sequences that are
 * whose indices are given by the bool vector.
 */
static int
write_setof_seqs (const char *seq_in_fname, const char *seq_out_fname,
                  const dist_mat &d_m, const vector<bool> &v_loved,
                  seq_index &s_i)
{
    const char *e_copying = "error copying sequences:";
    cout << __func__<< " Writing sequences from "<< seq_in_fname
         << " to "<< seq_out_fname << '\n';

    ofstream outfile (seq_out_fname);

    if (!outfile)
        return (bust(__func__, "Open fail", seq_out_fname,  ": ", strerror(errno), 0));
    try {
        for (unsigned i = 0; i < v_loved.size(); i++) {
            if (v_loved[i]) {
                string s = d_m.get_cmt(i);
                fseq fs = s_i.get_seq_by_cmmt(s);
                fs.clean(false);
                fs.write (outfile, 60);
            }
        }
    } catch (runtime_error &e) {
        return (bust(__func__, e_copying, e.what(), 0));}
    return EXIT_SUCCESS;
}

/* ---------------- write_unloved_seqs -----------------------
 */
static int
write_unloved_seqs (const char *unloved_fname, const dist_mat& d_m, const vector<bool>v_loved)
{
    ofstream outfile (unloved_fname);
    if (!outfile)
        return (bust(__func__, "Open fail on ", unloved_fname, ": ", strerror(errno), 0));
    for (unsigned i = 0; i < v_loved.size(); i++)
        if (v_loved[i] == false)
            outfile << d_m.get_cmt(i) << '\n';
    return EXIT_SUCCESS;
}

/* ---------------- get_loved --------------------------------
 * Given a vector of the right length set the elements if the
 * sequence is in the component.
 */
static void
get_loved (const component &cmpnt,  vector<bool> &v_loved)
{
    for (edge e: cmpnt.get_edges()) {
        v_loved[e.m1] = true;
        v_loved[e.m2] = true;
    }
}

/* ---------------- main  ------------------------------------
 */
int
main ( int argc, char *argv[])
{
    const char *progname = argv[0],
               *path_seq_fname    = NULL,
               *special_seq_fname = NULL,
               *mat_in_fname      = NULL,
               *seq_in_fname      = NULL,
               *seq_out_fname     = NULL, /* Where we write selected seqs */
               *unloved_fname = NULL; /* Where we  write unloved seqs */
    int c;
    int n_arg = 2;
    while ((c = getopt (argc, argv, "p:s:u:")) != -1)
        switch (c)
            {
            case 'p': path_seq_fname = optarg;                     break;
            case 's': seq_out_fname  = optarg;                     break;
            case 'u': unloved_fname  = optarg;                     break;
            case '?': return (usage(progname, "unknown option"));
            }
#   ifdef debug_till_i_vomit
    for (int index = optind; index < argc; index++)
        cout<< "next arg "<< argv[index]<< '\n';
#   endif /* debug_till_i_vomit */

    if (seq_out_fname)
        n_arg++;

    if ((argc - optind) < n_arg)
        return (usage(progname, "Too few arguments"));

    special_seq_fname     = argv[optind++];
    mat_in_fname  = argv[optind++];
    seq_in_fname = argv[optind++];

    vector<string> v_spec_seqs;

    if (get_spec_seqs (special_seq_fname, v_spec_seqs) < 2) {
        cerr<< "Too few sequences found in "<< special_seq_fname <<'\n';
        return EXIT_FAILURE;
    }
    dist_mat d_m(mat_in_fname);
    if (d_m.fail())
        return EXIT_FAILURE;
    vector<unsigned> v_spec_ndx;
    if (get_special_seq_ndx(d_m.get_cmt_vec(), v_spec_seqs, v_spec_ndx) == EXIT_FAILURE)
        return EXIT_FAILURE;
    component cmpnt = get_edges (v_spec_ndx, d_m.get_dist());
    cmpnt.describe (d_m);

    seq_index s_i;
    if (seq_out_fname || path_seq_fname) /* both of these might need a seq_index */
        if (s_i(seq_in_fname) == EXIT_FAILURE)
            return EXIT_FAILURE;

    {
        path path = dijkstra (cmpnt, d_m, v_spec_ndx); /* Outside of this little */
        path.print (nullptr, d_m);                     /* section, we do not need */
        if (path_seq_fname) {                          /* to store the path */
            if (path.write_seqs (path_seq_fname, d_m, s_i) == EXIT_FAILURE)
                return EXIT_FAILURE;
        }
    }

    vector<bool> v_loved (d_m.get_n_mem(), false);
    if (seq_out_fname || unloved_fname)
        get_loved (cmpnt, v_loved);
    if (seq_out_fname)
        if (write_setof_seqs (seq_in_fname, seq_out_fname, d_m, v_loved, s_i) == EXIT_FAILURE)
            return EXIT_FAILURE;
    if (unloved_fname)
        if (write_unloved_seqs (unloved_fname, d_m, v_loved) == EXIT_FAILURE)
            return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
