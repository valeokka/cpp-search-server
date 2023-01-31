#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <algorithm>
#include <set>
#include <tuple>
#include <numeric>
#include <stdexcept>
#include "document.h"
#include "string_processing.h"
#include "search_server.h"
using namespace std::string_literals;

void SearchServer::AddDocument(int document_id, const std::string &document, DocumentStatus status,
                               const std::vector<int> &ratings)
{
    if ((document_id < 0) || (documents_.count(document_id) > 0))
    {
        throw std::invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const std::string &word : words)
    {
        document_words[document_id][word] += inv_word_count;
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    document_ids_.push_back(document_id);
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
}

std::vector<Document> SearchServer::FindTopDocuments(
    const std::string &raw_query, DocumentStatus status) const
{
    return FindTopDocuments(raw_query,
                            [status](int document_id, DocumentStatus document_status, int rating)
                            {
                                return document_status == status;
                            });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string &raw_query) const
{
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const
{
    return document_ids_.size();
}

std::vector<int>::const_iterator SearchServer::begin() const
{
	return document_ids_.begin();
}

std::vector<int>::const_iterator SearchServer::end() const
{
	return document_ids_.end();
}

std::vector<int>::iterator SearchServer::begin()
{
	return document_ids_.begin();
}

std::vector<int>::iterator SearchServer::end()
{
	return document_ids_.end();
}

const std::map<std::string, double   > &SearchServer::GetWordFrequencies(int document_id) const
{   
    if (document_words.count(document_id) == 0){
       static std::map<std::string, double> empty_result;
        return empty_result;
    }
    return document_words.at(document_id);
}
 
// НЕ ЗАБУДЬ ПРОВЕРИТЬ ЧТО БЫСТРЕЕ

void SearchServer::RemoveDocument(int document_id)
{   
    {
    auto it = find(document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(it);
    }
    std::vector <std::string> words;
    documents_.erase(document_id);
    for (auto it = document_words[document_id].begin(); it != document_words[document_id].end(); ++it)
    {
        words.push_back(it->first);
    }
    for (const std::string& word: words){
    word_to_document_freqs_[word].erase(document_id);
    }
    document_words.erase(document_id);
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(
    const std::string &raw_query, int document_id) const
{
    const auto query = ParseQuery(raw_query);

    std::vector<std::string> matched_words;
    for (const std::string &word : query.plus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id))
        {
            matched_words.push_back(word);
        }
    }
    for (const std::string &word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id))
        {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const std::string &word) const
{
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string &word)
{
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c)
                   { return c >= '\0' && c < ' '; });
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string &text) const
{
    std::vector<std::string> words;
    for (const std::string &word : SplitIntoWords(text))
    {
        if (!IsValidWord(word))
        {
            throw std::invalid_argument("Word "s + word + " is invalid"s);
        }
        if (!IsStopWord(word))
        {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int> &ratings)
{
    if (ratings.empty())
    {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);

    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string &text) const
{
    if (text.empty())
    {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string word = text;
    bool is_minus = false;
    if (word[0] == '-')
    {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word))
    {
        throw std::invalid_argument("Query word "s + text + " is invalid");
    }

    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string &text) const
{
    Query result;
    for (const std::string &word : SplitIntoWords(text))
    {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop)
        {
            if (query_word.is_minus)
            {
                result.minus_words.insert(query_word.data);
            }
            else
            {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const std::string &word) const
{
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
