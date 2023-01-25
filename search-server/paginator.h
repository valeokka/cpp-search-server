#pragma once
#include <vector>
#include "string_processing.h"
template <typename It>
class Paginator{
        public:

    Paginator(It begin, It end, int size)
        : page_size (size){
        int cont_size = distance(begin,end); 
        for(int i = 0; i < cont_size; i += page_size){
        if (cont_size - i <= page_size){
            page_size = cont_size;
        }
        pages_.push_back(IteratorRange(begin += i, begin + page_size));
    } 
    }
    auto begin() const {
        return pages_.begin();
    }
    auto end() const {
        return pages_.end();
    }    
    int size() const {
        return page_size;
    }
 
    private:
    int page_size;
    std::vector<IteratorRange<It>> pages_;
    }; 

template <typename Container>
auto Paginate(const Container& c, int page_size) {
    return Paginator(begin(c), end(c), page_size);
}