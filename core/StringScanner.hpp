#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

namespace edb_next {

class DebugSession;

struct StringItem {
    Address address{0};
    std::string text;
    size_t length{0};
    std::string regionName;
    std::vector<Address> references; // Code locations referencing this string
};

class StringScanner {
public:
    static std::vector<StringItem> scan(DebugSession& session, size_t min_length = 4, size_t max_results = 5000);
};

} // namespace edb_next
