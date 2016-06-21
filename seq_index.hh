/*
 * 14 Jan 2016
 * This is a class which is born by reading a file full of sequences and storing
 * each comment with its position in the file.
 * We can then retrieve individual sequences using their comment as an index.
 *
 * Can only be included after <string>, <streampos>.
 */
/* ---------------- seq_index --------------------------------
 * Go to a file, read up each sequence and get an index
 */
class fseq;
class seq_index
{
public:
    typedef std::pair<std::string, std::streampos> line_read;
    typedef std::unordered_map<std::string, std::streampos > seq_map; 
private:
    std::ifstream infile; /* This is a real file handle. The file will
                      * be closed when the seq_index goes away */
    unsigned get_one (line_read *);
    seq_map s_map;
    std::string fname; /* so we can print error messages with file name */
    int fill (const char *fname);
 public:
    seq_index(){}
    seq_index (const char *fn) {fill (fn);}
    int operator ()(const char *fn) {return (fill (fn));}
    std::streampos get_pos_bynum (std::string s) { return s_map[s];}
    fseq get_seq_by_num (unsigned ndx);
    fseq get_seq_by_cmmt (const std::string &);

    size_t size() { return s_map.size();}

    seq_map::const_iterator begin() { return s_map.begin();}
    seq_map::const_iterator end() { return s_map.end();}
};
