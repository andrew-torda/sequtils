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
    const size_t get_size() { return seq.size() ;}
    bool fill (std::ifstream &infile, const size_t len_exp);
private:
    std::string cmmt;
    std::string seq;
    bool inner_fill (std::ifstream &infile, size_t len_exp);
};
#ifdef __clang__
#    pragma clang diagnostic pop
#endif /* clang */

/* ---------------- fseq_prop --------------------------------
 * What do we want to know about a sequence that might be
 * interesting ?
 * Has it been declared holy ? (To be kept)
 * How long is it ?
 * How many gap characters does it have ?
 * the constructor eats a sequence and fills out the information.
 * The idea is, we will read sequences, then put them in a queue.
 * the queue will spit out comment / property pairs.
 */
#ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wpadded"
#endif /* clang */
class fseq_prop {
public:
    fseq_prop () {ngap = 0; sacred = false; keep = false;}
    fseq_prop (fseq&);
    
    const bool is_sacred() const  { return sacred;}
    void make_sacred ()    { sacred = true; }
    const bool to_keep ()  { return keep;}
    unsigned int ngap;
private:
    bool sacred;
    bool keep;
};

#ifdef __clang__
#    pragma clang diagnostic pop
#endif /* clang */

/* ---------------- fseq_prop --------------------------------
 */




#endif /* FSEQ_H */
