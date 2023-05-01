/* 15 May 2023
 * This is part of reduce.
 * If we want to write a file with distances and the number of sequences
 * it goes through this file. We store state here. Things like the open file
 * handle.
 */

#include <fstream>
#include "plot_dist_reduce.hh"

static bool do_plot = false;
std::ofstream plot_file;

/* ---------------- distplot  --------------------------------
 * This might be called after we remove a sequence. We want to be able
 * to plot how the smallest value left in the distance matrix changes
 * as we go down the list. That is, we want a plot of
 *    dist n_sequences_left
 * We store all state here. At program start, we might call this function
 * and ask it to open the file. We can also call it to close the file.
 */
void
distplot ( unsigned long nseq, float dist) {
    if (do_plot == false)
        return;
    plot_file << nseq << " " << dist << "\n";
}

void
distplot_setup (const char *plot_fname) {
    plot_file.exceptions ( std::ifstream::failbit | std::ifstream::badbit );
    plot_file.open (plot_fname, std::ios::out);                                 
    do_plot = true;
}

void
distplot_close () {
    if (do_plot)
        plot_file.close();
}
