/*
 * 14 Jan 2016
 */

#include <iostream>
#include <cstdarg>
#include <string>

#include "bust.hh"
/* ---------------- bust  ------------------------------------
 * Something broke.
 * Print out the name of the function and then a series of const char *args.
 * The caller must terminate the list with a zero (0).
 */

void
bust_void (const char *func, ...)
{
    std::string errmsg = std::string (func) + ": ";
    va_list tl;
    va_start (tl, func);
    const char *t = va_arg (tl, const char *);
    while (t != 0) {
        errmsg += t; errmsg += ' ';
        t = va_arg (tl, const char *);
    }
    va_end (tl);
    errmsg += '\n';
    std::cerr << errmsg;
}

int
bust (const char *func, ...)
{
    va_list sl;
    std::string errmsg = std::string (func) + ": ";
    va_start (sl, func);
    for ( const char *s = va_arg (sl, const char *); s; s = va_arg (sl, const char *)) {
        errmsg += s;
        errmsg += ' ';
    }
    va_end (sl);
    errmsg += '\n';
    std::cerr << errmsg;
    return EXIT_FAILURE;
}
