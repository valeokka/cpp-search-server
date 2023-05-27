#include <vector>
#include <deque>
#include <map>
#include <string>
#include <cmath>
#include <algorithm>
#include <set>
#include <tuple>
#include <execution>
#include <string_view>
#include <numeric>
#include <stdexcept>
#include "document.h"
#include "string_processing.h"
#include "search_server.h"
using namespace std::string_literals;

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status,
                                                                        const std::vector<int> &ratings){
    if ((document_id < 0) || (documents_.count(document_id) > 0)){
        throw std::invalid_argument("Invalid document_id"s);
    }

    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (std::string_view word : words){
        words_.push_back(std::move(std::string{word}));
        word_to_document_freqs_[words_.back()][document_id] += inv_word_count;
        document_words[document_id][words_.back()] += inv_word_count;
    }
    document_ids_.push_back(document_id);
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
}
    
    //Normal FTD

std::vector<Document> SearchServer::FindTopDocuments(
    std::string_view raw_query, DocumentStatus status) const{
        return FindTopDocuments(std::execution::seq,raw_query, status);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const{
    return FindTopDocuments(std::execution::seq,raw_query);
}

int SearchServer::GetDocumentCount() const{
    return document_ids_.size();
}

std::deque<int>::const_iterator SearchServer::begin() const{return document_ids_.begin();}
std::deque<int>::const_iterator SearchServer::end() const{return document_ids_.end();}
std::deque<int>::iterator SearchServer::begin(){return document_ids_.begin();}
std::deque<int>::iterator SearchServer::end(){return document_ids_.end();}

const std::map<std::string_view, double> &SearchServer::GetWordFrequencies(int document_id) const{   
    if (document_words.count(document_id) == 0){
        static std::map<std::string_view, double> empty_result;
        return empty_result;
    }
    return document_words.at(document_id);
}

void SearchServer::RemoveDocument(int document_id){
    {
    auto it = find(document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(it);
    }
    std::vector <std::string> words;
    documents_.erase(document_id);
    auto it = document_words[document_id].begin();
    while(it != document_words[document_id].end()){
        words.push_back(std::string{it->first});
        ++it;
    }
    for (const auto& [word, d] : document_words[document_id]){
        word_to_document_freqs_[word].erase(document_id);
    };
    document_words.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&,int document_id){   
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id){   
    {
    auto it = find(std::execution::par,document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(it);
    }
    documents_.erase(document_id);
    std::for_each(std::execution::par,document_words[document_id].begin(), 
                     document_words[document_id].end(), [&](auto& items){
       word_to_document_freqs_[items.first].erase(document_id);
    });
//
    document_words.erase(document_id);
}

SearchServer::MatchedDoc SearchServer::MatchDocument(std::string_view raw_query, int document_id) const{
    const auto query = ParseQuery(std::execution::seq,raw_query);
    
    std::vector<std::string_view> matched_words;
        for (std::string_view w : query.minus_words){
        if (word_to_document_freqs_.count(w) != 0 && word_to_document_freqs_.at(w).count(document_id)){
            return {matched_words, documents_.at(document_id).status};
        }
    }
    for (std::string_view w : query.plus_words){
        if (word_to_document_freqs_.count(w) != 0 && word_to_document_freqs_.at(w).count(document_id)){
            matched_words.push_back(w);
        }
    }
        std::sort(matched_words.begin(),matched_words.end());
    matched_words.erase(std::unique(matched_words.begin(),matched_words.end()),matched_words.end());
    return {matched_words, documents_.at(document_id).status};
}

SearchServer::MatchedDoc SearchServer::MatchDocument(const std::execution::sequenced_policy&,std::string_view raw_query, 
                                                                                    int document_id) const{
   return MatchDocument(raw_query, document_id);
}

SearchServer::MatchedDoc SearchServer::MatchDocument(const std::execution::parallel_policy&,std::string_view raw_query, 
                                                                                int document_id) const{
    const auto query = ParseQuery(std::execution::par,raw_query);

    std::vector<std::string_view> matched_words;
    if(std::any_of(std::execution::par,query.minus_words.begin(),query.minus_words.end(),[this, document_id](const auto& str){
        return word_to_document_freqs_.count(str) != 0 && word_to_document_freqs_.at(str).count(document_id)!= 0;}))
    { 
            return {matched_words, documents_.at(document_id).status};
    }

    matched_words.reserve(query.plus_words.size());
    auto last_copy = std::copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(),
    [this,document_id](const auto& w){
    return word_to_document_freqs_.count(w) != 0 && word_to_document_freqs_.at(w).count(document_id);
    });
    matched_words.erase(last_copy,matched_words.end());
    std::sort(matched_words.begin(),matched_words.end());
    matched_words.erase(std::unique(matched_words.begin(),matched_words.end()),matched_words.end());
    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(std::string_view word) const{
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(std::string_view word){
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c)
                   { return c >= '\0' && c < ' '; });
}

std::deque<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const{
    std::deque<std::string_view> words;
    for (const auto &word : SplitIntoWords(text)){
        if (!IsValidWord(word)){
            throw std::invalid_argument("Word  is invalid"s);
        }
        if (!IsStopWord(word)){
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int> &ratings){
    if (ratings.empty()){
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);

    return rating_sum / static_cast<int>(ratings.size());
}



SearchServer::Query SearchServer::ParseQuery(const std::execution::sequenced_policy&, std::string_view text) const{
    Query result;
        auto words = SplitIntoWords(text);
        result.minus_words.reserve(words.size());
        result.plus_words.reserve(words.size());
        std::sort(words.begin(),words.end());
        words.erase(std::unique(words.begin(),words.end()),words.end());

    std::for_each(words.begin(),words.end(),[this, &result](std::string_view  word){
        const auto query_word = ParseQueryWord(word);
        
        if (!query_word.is_stop){
            query_word.is_minus ?   result.minus_words.push_back(query_word.data) : 
                                    result.plus_words.push_back(query_word.data);
        }
    });
    result.minus_words.shrink_to_fit();
    result.plus_words.shrink_to_fit();
    return result;
}

SearchServer::Query SearchServer::ParseQuery(const std::execution::parallel_policy&,std::string_view text) const{
    Query result;
    auto words = SplitIntoWords(text);
    result.minus_words.reserve(words.size());
    result.plus_words.reserve(words.size());
    std::sort(words.begin(),words.end());
    words.erase(std::unique(words.begin(),words.end()),words.end());
    for (std::string_view word : words){
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop){
            query_word.is_minus ?   result.minus_words.push_back(query_word.data) : 
                                    result.plus_words.push_back(query_word.data);
        }
    }
    result.minus_words.shrink_to_fit();
    result.plus_words.shrink_to_fit();
    return result;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const{
    if (text.size() == 0){
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string_view word =text;
    bool is_minus = false;
    if (word[0] == '-'){
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)){
        throw std::invalid_argument("Query word is invalid");
    }

    return {word, is_minus, IsStopWord(word)};
}
// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const{
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
