/* 15 May 2023
 * This is part of reduce.
 * If we want to write a file with distances and the number of sequences
 * it goes through this file. We store state here. Things like the open file
 * handle.
 */

#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include "plot_dist_reduce.hh"

static bool do_plot = false;
static std::ofstream plot_file;
static std::ostream  *out;

/* ---------------- distplot  --------------------------------
 * This might be called after we remove a sequence. We want to be able
 * to plot how the smallest value left in the distance matrix changes
 * as we go down the list. That is, we want a plot of
 *    dist n_sequences_left
 * We store all state here. At program start, we might call this function
 * and ask it to open the file. We can also call it to close the file.
 * We write to "out", which might be a pointer to standard output or a file
 * we have opened.
 */
void
distplot ( const unsigned long nseq, const float dist) {
    if (do_plot == false)
        return;
    *out << nseq << " " << std::setprecision(6) << dist << "\n";
}

/* ---------------- distplot_setup ---------------------------
 * Open a file or set out to point to standard out, or maybe just
 * return if there is nothing to do.
 */
void
distplot_setup (const char *plot_fname) {
    plot_file.exceptions ( std::ifstream::failbit | std::ifstream::badbit );
    if (std::strcmp ("-", plot_fname) == 0) {
        out = &std::cout;
    } else {
        plot_file.open (plot_fname, std::ios::out);
        out = &plot_file;
    }
    do_plot = true;
}

void
distplot_close () {
    if (do_plot)
        if (plot_file.is_open())
            plot_file.close();
}
