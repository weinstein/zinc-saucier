#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <string>
#include <vector>

namespace util {

std::string StringToLower(const std::string& str);
bool StringToInt(const std::string& str, int* out);
bool StringToLong(const std::string& str, long* out);
bool StringToDouble(const std::string& str, double* out);

std::vector<std::string> SplitString(const std::string& str, const std::string& delim);

};  // namespace util

#endif
