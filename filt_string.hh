/* 22 Jun 2016
 */
#ifndef FILT_STRING_HH
#define FILT_STRING_HH

void set_vec (const std::string &s, std::vector<bool> &v);
std::string squash_string_vec (std::string &s, const std::vector<bool> &v);
std::string rmv_white_start_end (std::string &s);

#define FILT_STRING_SIZE_WRONG 

#endif /* FILT_STRING_HH */
