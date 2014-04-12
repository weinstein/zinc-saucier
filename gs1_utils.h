#ifndef GS1_UTILS_H
#define GS1_UTILS_H

#include "gs1.pb.h"
#include "string_utils.h"

namespace zns {

GS1Product PODRowToGS1Product(const std::vector<std::string>& row);
Brand PODRowToBrand(const std::vector<std::string>& row);

};  // namespace zns

#endif
