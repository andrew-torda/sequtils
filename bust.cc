/*
 * 14 Jan 2016
 */

#include <iostream>
#include "bust.hh"
/* ---------------- bust  ------------------------------------
 * Something broke.
 */
int
bust (const char *func, const char *e_msg)
{
    std::cerr << func << ": " << e_msg << '\n';
    return EXIT_FAILURE;
}
int
bust (const char *func, const std::string &e_msg)
{
    std::cerr << func << ": " << e_msg << '\n';
    return EXIT_FAILURE;
}
