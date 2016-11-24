/*
 * 21 Nov 2016
 * Read a sequence, start and end residues.
 * print out the sequence from start to end.
 */

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <regex>
#include <string>
#include <unistd.h>   /* non standard, but for getopt() */

#include "bust.hh"
#include "fseq.hh"

using namespace std;

/* ---------------- usage ------------------------------------ */
static int usage ( const char *progname, const char *s)
{
    static const char *u
        = " [-o outfile] [-n ] infile n_start n_end\n";
    return (bust(progname, s, "\n", progname, u, 0));
}

/* ---------------- s_to_uint --------------------------------
 * wrapper around stoul giving an unsigned int, but also
 * printing out a better error message on failure.
 * This is pretty crappy. I tried feeding a negative number to
 * stoul(). It does not complain !
 */
static int
s_to_uint (const char *s, unsigned int &i)
{
    try {
        i = stoul (s);
    } catch (const std::invalid_argument &ia) {
        cerr << "unable to convert "<< s << " to unsigned int\n";
        return (EXIT_FAILURE);
    } catch (const std::out_of_range &ia) {
        cerr << s << " could not be converted to unsigned int, out of range\n";
        return (EXIT_FAILURE);
    }
    
    return (EXIT_SUCCESS);
}

/* ---------------- add_seq_num ------------------------------
 * Given
 *   > xp_14324.1 protein doing blah [mus musculus]
 * and a pair of numbers 3 and 5, change the string so it becomes
 *   > xp_14324.1/3-5 protein doing blah [mus musculus]
 * If the match fails, we just append the string to the end. 
 */
static string
add_seq_num (string &s, const unsigned i_start, const unsigned i_end)
{
    string s_to_add = "/" + to_string(i_start) + "-" + to_string(i_end);
    regex pattern ("^>\\s*\\S+\\s");
    smatch match;
    
    if (regex_search (s, match, pattern))
        s.insert (match[0].length() - 1, s_to_add);
    else
        s = s + s_to_add;
    return s;
}

/* ---------------- main  ------------------------------------ */
int
main (int argc, char *argv[])
{
    const char * progname = argv[0];
    const char * ofile_name = NULL;
    const char * infile_name = NULL;
    unsigned int i_start, i_end;
    int c;
    unsigned short int verbosity = 0;
    bool add_num_flag  = true;
    bool eflag         = false;
    bool last_res_flag = false;
    while (( c = getopt (argc, argv, "+o:")) != -1) {
        switch (c) {
        case 'n':           /* do not add seq numbers to comment line */
            add_num_flag = false;
        case 'o':           /* output file name */
            ofile_name = optarg;           break;
        case 'v':           /* verbosity */
            verbosity++;                       break;
        case '?':
            cerr << argv[0] << "unknown option\n";  eflag = true; break;
        }
    }
    if (eflag)
        return (usage(progname, ""));
    if (argc - optind  <   3)
        return (usage (progname, "too few arguments"));
    if (argc - optind >  3)
        return (usage (progname, "too many arguments"));
    argv += optind;
    infile_name = argv[0];
    if (s_to_uint (argv[1], i_start) == EXIT_FAILURE)
        return (bust(progname, "stopping", 0));
    if (string(argv[2]) == string("end"))
        last_res_flag = true;
    else if (s_to_uint (argv[2], i_end)   == EXIT_FAILURE)
        return (bust(progname, "stopping", 0));

    if (i_end < i_start)
        swap (i_end, i_start);

    cout << "I want to read from "<< infile_name <<
        " with args "<< i_start << " and "<< i_end << '\n';
    if (ofile_name)
        cout << "writing to "<< ofile_name << '\n';
    if (add_num_flag)
        cout << "will append seq numbers to comments"<< '\n';
    
    ifstream infile (infile_name);
    
    if (!infile)
        return (bust(progname, "open fail on ", infile_name, ": ", strerror(errno), 0));
    ofstream outfilestr;
    ostream *out_p;
    
    if (ofile_name) {
        outfilestr.open (ofile_name);
        if (!outfilestr)
            return (bust(progname, "open fail on ", ofile_name, " for writing: ",
                         strerror(errno), 0));
        out_p = & outfilestr;
    } else {
        out_p = & cout;
    }

    fseq seq(infile, 0);
    {
        const bool keep_gap = false;
        const bool remove_white = true;
        seq.clean (keep_gap, remove_white);
    }
    {
        const size_t ll = seq.get_size();
        if (verbosity > 0)
            cout << "input seq length "<< ll << '\n';
        if (last_res_flag)
            i_end = ll;
        if (ll == 0)
            return (bust(progname, "empty sequence in file ", infile_name, 0));
        if ( (i_end - 1) > ll)
            return (bust(progname, "asked for ", argv[2],
                         " residues, but sequence only has ", to_string(ll).c_str(),
                         " residues", 0));
    }

    string s = seq.get_seq();
    s = s.substr(i_start - 1, i_end - i_start + 1);
    seq.replace_seq ( s );
    if (add_num_flag) {
        string cmmt = seq.get_cmmt();
        seq.replace_cmmt (add_seq_num (cmmt, i_start, i_end));
    }
    seq.write ( *out_p, 60);
    if (ofile_name)
        outfilestr.close();

    infile.close();
    return EXIT_SUCCESS;
}
