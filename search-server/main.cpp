#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double MAGICNUM = 1e-6;
string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            words.push_back(word);
            word = "";
        }
        else {
            word += c;
        }
    }
    words.push_back(word);

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < MAGICNUM) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status_) const {

        return FindTopDocuments(raw_query, [status_](int document_id, DocumentStatus status, int rating) { return status == status_; });

    }
    vector<Document> FindTopDocuments(const string& raw_query) {

        return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });

    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }
};


/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr),#expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr),#expr, __FILE__, __FUNCTION__, __LINE__, (hint))


template <typename a, typename b>
void RunTestImpl(a func1, b func2) {
    func1();
    cerr << func2 << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)  // напишите недостающий код

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}

void TestAddDocument() {
    const int doc_id0 = 1;
    const string content0 = "dog with orange orange"s;
    const vector<int> ratings0 = { 1, 2, 3, 4 };
    const int doc_id1 = 3;
    const string content1 = "orange zhopa"s;
    const vector<int> ratings1 = { 2 };
    const int doc_id2 = 1;
    const string content2 = "cat with flowers"s;
    const vector<int> ratings2 = { 2, 3, 10 };

    {
        SearchServer server;
        server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings0);
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);

        ASSERT(server.GetDocumentCount() == 2);
    }
    {
        SearchServer server;
        server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings0);
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(15, content2, DocumentStatus::ACTUAL, ratings2);

        ASSERT(server.GetDocumentCount() == 3);
    }
    {
        SearchServer server;
        server.AddDocument(0, "", DocumentStatus::ACTUAL, ratings0);
        ASSERT(server.GetDocumentCount() == 1);
    }
    {
        SearchServer server;
        server.SetStopWords("gay");
        server.AddDocument(0, "gay", DocumentStatus::ACTUAL, ratings0);
        ASSERT(server.GetDocumentCount() == 1);
    }

}

void TestMinusWords() {
    {
        SearchServer server;
        server.AddDocument(1, "dog in space", DocumentStatus::ACTUAL, { 2 });
        server.AddDocument(3, "dog in blue shirt", DocumentStatus::ACTUAL, { 2 });
        auto FoundedDocs = server.FindTopDocuments("dog -shirt");
        ASSERT(FoundedDocs.size() == 1);
    }
    {
        SearchServer server;
        server.AddDocument(1, "dog in space", DocumentStatus::ACTUAL, { 2 });
        server.AddDocument(3, "dog shirt", DocumentStatus::ACTUAL, { 2 });
        auto FoundedDocs = server.FindTopDocuments("-dog -shirt");
        ASSERT(FoundedDocs.size() == 0);
    }
}

void TestStatus() {
    //Поиск по статусу
    {
        SearchServer server;
        server.AddDocument(1, "dog in space", DocumentStatus::BANNED, { 2 });
        server.AddDocument(3, "dog in blue shirt", DocumentStatus::ACTUAL, { 2 });
        server.AddDocument(5, "dog in blue shirt1", DocumentStatus::ACTUAL, { 2 });
        server.AddDocument(4, "dog in blue shirt2", DocumentStatus::ACTUAL, { 2 });
        auto docs1 = server.FindTopDocuments("dog", DocumentStatus::BANNED);
        ASSERT(docs1.size() == 1);
        auto docs2 = server.FindTopDocuments("dog", DocumentStatus::ACTUAL);
        ASSERT(docs2.size() == 3);
        auto docs3 = server.FindTopDocuments("dog", DocumentStatus::IRRELEVANT);
        ASSERT(docs3.size() == 0);
        auto docs4 = server.FindTopDocuments("dog", DocumentStatus::REMOVED);
        ASSERT(docs4.size() == 0);
    }
    //поиск если стандартный статус = актуальный
    {
        SearchServer server;
        server.AddDocument(1, "dog in space", DocumentStatus::BANNED, { 2 });
        server.AddDocument(3, "dog in blue shirt", DocumentStatus::ACTUAL, { 2 });
        auto docs = server.FindTopDocuments("dog");
        ASSERT(docs.size() == 1);

    }
    //нет подходящих слов
    {
        SearchServer server;
        server.AddDocument(1, "dog in space", DocumentStatus::BANNED, { 2 });
        server.AddDocument(3, "dog in blue shirt", DocumentStatus::ACTUAL, { 2 });
        auto docs1 = server.FindTopDocuments("eeee", DocumentStatus::BANNED);
        ASSERT(docs1.size() == 0);
        auto docs2 = server.FindTopDocuments("eeee", DocumentStatus::ACTUAL);
        ASSERT(docs2.size() == 0);
        auto docs3 = server.FindTopDocuments("eeee", DocumentStatus::IRRELEVANT);
        ASSERT(docs3.size() == 0);
        auto docs4 = server.FindTopDocuments("eeee", DocumentStatus::REMOVED);
        ASSERT(docs4.size() == 0);
    }
}
void TestRelevance() {
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
        auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT(docs[0].rating == 5);
        ASSERT(abs(docs[0].relevance - 0.866434) <= MAGICNUM);
    }
    
}
void TestSort() {
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
        auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT(docs[0].id == 1);
        ASSERT(docs[1].id == 0);
        ASSERT(docs[2].id == 2);
    }
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
        auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT(docs[0].relevance >= docs[1].relevance && docs[1].relevance >= docs[2].relevance);
    }
}
void TestRating() {
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_EQUAL(docs[0].rating,2 );
    }
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { -8, -3 });
        auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_EQUAL(docs[0].rating, -5);
    }
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, 3 });
        auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_EQUAL(docs[0].rating, 5);
    }
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 0, 0 });
        auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_EQUAL(docs[0].rating, 0);
    }
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 3, -3 });
        auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_EQUAL(docs[0].rating, 0);
    }
}
void TestSearch() {
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 3, -3 });
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 0 });
        auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) 
            { return rating == 0; });        
        ASSERT(docs.size() == 2);
    }
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 3, -3 });
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
        search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 0 });
        auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id == 0; });
        ASSERT(docs.size() == 1);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestStatus);
    RUN_TEST(TestRelevance);
    RUN_TEST(TestSort);
    RUN_TEST(TestRating);
    RUN_TEST(TestSearch);
    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
