#pragma once
#include <set>
#include <string>
#include <string_view>
#include <deque>

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string, std::less<>> non_empty_strings{};
    for (const auto& sv : strings) { 
        if (!sv.empty()) { 
            non_empty_strings.insert(std::string{ sv });
        }
    }
    return non_empty_strings;
}

std::deque<std::string_view> SplitIntoWords(std::string_view str);

template <typename It>
struct IteratorRange{
    IteratorRange(It begin_in, It end_in)
    : begin (begin_in),
    end (end_in){}

    It begin;
    It end;
};