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
    fseq () {
        cmmt = "";
        seq = "";
    }
    fseq (std::ifstream &infile, size_t len_exp);
    
    const std::string get_cmmt( void ) { return cmmt ;}
    const std::string get_seq( void )  { return seq ;}
    bool replace (std::ifstream &infile, size_t len_exp);
private:
    bool inner_fill (std::ifstream &infile, size_t len_exp);
    std::string cmmt;
    std::string seq;
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
    fseq_prop () {length = 0; ngap = 0; sacred = false; keep = false;}
    fseq_prop (fseq&);
    bool is_sacred() { return sacred;}
    bool to_keep ()  { return keep;}
    size_t length;
    unsigned int ngap;
private:
    bool sacred;
    bool keep;
};

#ifdef __clang__
#    pragma clang diagnostic pop
#endif /* clang */

/* ---------------- pair_info --------------------------------
 */
struct pair_info {
public:
    class fseq_prop fseq_prop;
    class fseq fseq;
    pair_info (class fseq_prop &f_prop, class fseq &fs) {
        fseq_prop = f_prop;
        fseq = fs;
    }
    pair_info();
private:
};

/*
pair_info::pair_info()
{
    fseq = fseq();
    fseq_prop =fseq
    fseq_prop no_prop (no_seq);
    pair_info (no_prop, no_seq);
}

*/

#endif /* FSEQ_H */
