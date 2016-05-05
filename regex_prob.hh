/*
 * 5 May 2016
 * There is a problem with regex's and old compilers on our system.
 * If we use gcc, we get the old version which does not support regex's.
 * If we use clang++, it works, although there is some linker confusion
 * that makes the linking more complicated.
 * Here we decide if our version of gcc is old, to use the boost library.
 * If things seem newer, we just use regex.
 */
#ifndef REGEX_PROB_HH
#define REGEX_PROB_HH

#if (!(defined (__GNUC__)) ||  ((__GNUC__ * 10000 + __GNUC_MINOR__ * 100) >= 40900) || defined __clang__)
#    include <regex>
#    define regex_choice std
#    pragma message "Using regex from standards and not boost"
#else
#    include <boost/regex.hpp>
#    define regex_choice boost
#    pragma message "Using boost for regex functions"
#endif /* old gcc */


#endif /* REGEX_PROB_HH */
