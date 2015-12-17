/* 17 Dec 2015
 * Die on a fatal error.
 */


#include "prog_bug.hh"
#include <exception>
#include <stdexcept>
#include <string>

/* ---------------- prog_bug ---------------------------------
 */
[[noreturn]] void
prog_bug (const char *file, const int line, const char *msg)
{
    std::string s = file;
    const char *colon = ": ";
    s += colon;
    s += std::to_string(line);
    s += colon;
    s += msg;
    s += "\nStopping";
    throw (std::runtime_error(s));
}
