/* 10 oct 2015 */

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
    std::string const get_cmmt( void ) { return cmmt ;}
    std::string get_seq( void )  { return seq ;}
    size_t get_size() { return seq.size() ;}
    void replace_seq (const std::string s) {seq = s;}
    bool fill (std::ifstream &infile, const size_t len_exp);
private:
    std::string cmmt;
    std::string seq;
    bool inner_fill (std::ifstream &infile, size_t len_exp);
};
#ifdef __clang__
#    pragma clang diagnostic pop
#endif /* clang */


#endif /* FSEQ_H */
