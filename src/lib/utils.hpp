#pragma once

#include <regex>
#include <string>

namespace Utils {

static inline std::string leftTrim(const std::string &s) {
    return std::regex_replace(s, std::regex("^\\s+"), std::string(""));
}
 
static inline std::string rightTrim(const std::string &s) {
    return std::regex_replace(s, std::regex("\\s+$"), std::string(""));
}
 
static inline std::string trim(const std::string &s) {
    return leftTrim(rightTrim(s));
}

} // namespace Utils