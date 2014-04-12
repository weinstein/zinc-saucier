#include "string_utils.h"

#include <algorithm>
#include <string>
using std::string;
#include <vector>
using std::vector;

namespace util {

string StringToLower(const string& str) {
   string lower = str;
   std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
   return lower;
}

bool StringToInt(const string& str, int* out) {
   try {
      *out = std::stoi(str);
      return true;
   } catch (std::exception e) {
      return false;
   }
}

bool StringToLong(const string& str, long* out) {
   try {
      *out = std::stol(str);
      return true;
   } catch (std::exception e) {
      return false;
   }
}

bool StringToDouble(const string& str, double* out) {
   try {
      *out = std::stod(str);
      return true;
   } catch (std::exception e) {
      return false;
   }
}

vector<string> SplitString(const string& str, const string& delim) {
   vector<string> result;
   int idx = 0;
   while (idx < str.size()) {
      int jdx = str.find(delim, idx);
      if (jdx == string::npos) {
         result.push_back(str.substr(idx));
         break;
      } else {
         result.push_back(str.substr(idx, jdx - idx));
         idx = jdx + delim.size();
         if (idx == str.size()) {
            result.push_back("");
            break;
         }
      }
   }
   return result;
}

};  // namespace util
