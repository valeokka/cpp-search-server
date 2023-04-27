#include "string_processing.h"
#include <algorithm>

std::deque<std::string_view> SplitIntoWords(std::string_view str) {
    std::deque<std::string_view> result;
    const int64_t pos_end = str.npos;
    str.remove_prefix(std::min(str.size(), str.find_first_not_of(" ")));
    while (str.size() != 0) {
        int64_t space = str.find(' ');
        result.push_back(space == pos_end ? str.substr(0) : str.substr(0, space));
        str.remove_prefix(std::min(str.size(), str.find_first_not_of(" ", space)));
    }

    return result;
}