/* 19 Nov 2015
 */
#ifndef FSEQ_PROP_H
#define FSEQ_PROP_H
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

class fseq;

class fseq_prop {
public:
    fseq_prop () {ngap = 0; sacred = false; keep = false;}
    fseq_prop (fseq&);    
    bool is_sacred() const  { return sacred;}
    void make_sacred ()     { sacred = true; }
    bool to_keep ()         { return keep;}
    unsigned int ngap;
private:
    bool sacred;
    bool keep;
};

#ifdef __clang__
#    pragma clang diagnostic pop
#endif /* clang */

#endif /* FSEQ_PROP_H */
