/* 10 oct 2015
 * Can only be included after <fstream>.
 */

#ifndef FSEQ_H
#define FSEQ_H

/* ---------------- fseq  ------------------------------------
 * We call it a fasta seq (fseq), which means a sequence with
 * a comment.
 * We store comments untouched, with the ">" at the start.
 */
#ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wpadded"
#endif /* clang */

class fseq {
public:
    fseq (std::ifstream &infile, const size_t len_exp);
    fseq () {}
    void clean (const bool keep_gap, const bool rmv_white);
    std::string const get_cmmt( void ) { return cmmt ;}
    std::string const get_seq( void )  { return seq ;}
    size_t get_size() { return seq.size() ;}
    void replace_seq (const std::string s) {seq = s;}
    bool fill (std::ifstream &infile, const size_t len_exp);
    void write (std::ostream &ofile, const unsigned short line_len);

private:
    std::string cmmt;
    std::string seq;
    bool inner_fill (std::ifstream &infile, size_t len_exp);
};
#ifdef __clang__
#    pragma clang diagnostic pop
#endif /* clang */

#endif /* FSEQ_H */
