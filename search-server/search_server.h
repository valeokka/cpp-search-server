#pragma once
#include <vector>
#include <map>
#include <string>
#include <string_view>
#include <deque>
#include <set>
#include <tuple>
#include <stdexcept>
#include <algorithm>
#include <execution>
#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"
#include "log_duration.h"
#include <chrono>
#include <iostream>

constexpr double ACCURACY = 1e-6;
enum class DocumentStatus{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};
const int MAX_RESULT_DOCUMENT_COUNT = 5;


class SearchServer{
public:
    SearchServer() = default;

    explicit SearchServer(const std::string &stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {}

        explicit SearchServer(const std::string_view &stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {}

        template <typename StringContainer>
    explicit SearchServer(const StringContainer &stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)){
        if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)){
            throw std::invalid_argument("Some of stop words are invalid");
        }
    }

    void AddDocument(int document_id, const std::string_view &document, DocumentStatus status,
                     const std::vector<int> &ratings);

    int GetDocumentCount() const;

    const std::map<std::string_view, double> &GetWordFrequencies(int document_id) const;

    std::deque<int>::const_iterator begin() const;
    std::deque<int>::const_iterator end() const;
    std::deque<int>::iterator begin();
    std::deque<int>::iterator end();

    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
    const std::execution::sequenced_policy&,const std::string_view &raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
    const std::execution::parallel_policy&, const std::string_view &raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
                                            const std::string_view &raw_query, int document_id) const;
    //Sequenced FTD
            template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view &raw_query,
                                           DocumentPredicate document_predicate) const{
        const auto query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
            sort(matched_documents.begin(), matched_documents.end(),
                 [](const Document &lhs, const Document &rhs) {
                     if (std::abs(lhs.relevance - rhs.relevance) < ACCURACY) {
                         return lhs.rating > rhs.rating;
                     } else {
                         return lhs.relevance > rhs.relevance;
                     }
                 });
        
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT){
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy,
                    const std::string_view &raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, 
                                            const std::string_view &raw_query) const;
    
    //Normal FTD
    std::vector<Document> FindTopDocuments(const std::string_view &raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view &raw_query) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view &raw_query, DocumentPredicate document_predicate) const{
        return FindTopDocuments(std::execution::seq,raw_query, document_predicate);
    }


    //Parallel FTD
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy,
                    const std::string_view &raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, 
                                            const std::string_view &raw_query) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, 
    const std::string_view &raw_query, DocumentPredicate document_predicate) const{
        const auto query = ParseQuery(std::execution::par,raw_query);
        auto matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(), [](const Document &lhs, const Document &rhs){
                 if (std::abs(lhs.relevance - rhs.relevance) < ACCURACY){
                     return lhs.rating > rhs.rating;
                 }else{
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT){
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

private:
    struct DocumentData{
        int rating;
        DocumentStatus status;
    };

    struct Query{
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    struct QueryWord{
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_words;
    std::map<int, DocumentData> documents_;
    std::deque<int> document_ids_;
    std::deque<std::string> words_;

    bool IsStopWord(const std::string_view &word) const;
    static bool IsValidWord(const std::string_view &word);

    std::deque<std::string_view> SplitIntoWordsNoStop(const std::string_view &text) const;

    static int ComputeAverageRating(const std::vector<int> &ratings);

    Query ParseQuery(const std::string_view &text) const;
    Query ParseQuery(const std::execution::parallel_policy&,const std::string_view &text) const;

    QueryWord ParseQueryWord(const std::string_view &text) const;
    
    double ComputeWordInverseDocumentFreq(const std::string_view &word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query &query,
                                           DocumentPredicate document_predicate) const{
        std::map<int, double> document_to_relevance;
            for (const std::string_view &word : query.plus_words){
                if (word_to_document_freqs_.count(word) == 0){continue;}

                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)){
                    const auto &document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)){
                        document_to_relevance[document_id] += term_freq * inverse_document_freq;
                    }   
                }
            }

        for (const std::string_view &word : query.minus_words){
            if (word_to_document_freqs_.count(word) != 0){
                for (const auto [document_id, _] : word_to_document_freqs_.at(word)){
                    document_to_relevance.erase(document_id);
                    }
                }
            }

        std::vector<Document> matched_documents;
            for (const auto [document_id, relevance] : document_to_relevance){
                matched_documents.push_back(
                        {document_id, relevance, documents_.at(document_id).rating});
            }

        return matched_documents;
    }

        template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy,const Query &query,
                                           DocumentPredicate document_predicate) const{
        ConcurrentMap<int, double> document_to_relevance(100);
        for_each(std::execution::par, query.plus_words.begin(),query.plus_words.end(), 
                [&](const std::string_view &word ){
            if (word_to_document_freqs_.count(word) != 0){
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for_each (word_to_document_freqs_.at(word).begin(),word_to_document_freqs_.at(word).end(),
                [&](auto it){
                    const auto &document_data = documents_.at(it.first);
                if (document_predicate(it.first, document_data.status, document_data.rating)){
                       document_to_relevance[it.first].ref_to_value += it.second * inverse_document_freq;
                    }
                }  
                );
            }
        });

        for_each(query.minus_words.begin(),query.minus_words.end(),
        [&](const std::string_view &word){
            if (word_to_document_freqs_.count(word) != 0){
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)){
                document_to_relevance.Delete(document_id);
            }
            }
        });
                
                auto documents_map = document_to_relevance.BuildOrdinaryMap();
        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : documents_map){   
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }

        return matched_documents;
    }
};

