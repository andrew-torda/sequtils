/*
 * 14 Jan 2016
 * Include after <string>
 */

#ifndef BUST_HH
#define BUST_HH


void bust_void (const char *func, const std::string &e_msg);
int bust (const char *func, const char *s...);
void bust_void (const char *func, const char *s, ...);

#endif /* BUST_HH */
