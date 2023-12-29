/*
 * Copyright (C) 2024 Pawel Maslanka (pawmas@hotmail.com)
 * This program is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, version 3.
 */
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
 
static inline std::string Trim(const std::string &s) {
    return leftTrim(rightTrim(s));
}

static void find_and_replace_all(String& data, String to_search, String replace_str) {
    // Get the first occurrence
    size_t pos = data.find(to_search);
    // Repeat till end is reached
    while (pos != String::npos) {
        // Replace this occurrence of Sub String
        data.replace(pos, to_search.size(), replace_str);
        // Get the next occurrence from the current position
        pos = data.find(to_search, pos + replace_str.size());
    }
}

} // namespace Utils