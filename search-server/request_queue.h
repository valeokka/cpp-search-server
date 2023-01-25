#pragma once
#include <string>
#include <vector>
#include <deque>
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) 
    : search_server(search_server)
    {    
}
    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    void AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        std::vector <Document> result = search_server.FindTopDocuments(raw_query, document_predicate);
        PushDocument(result);
    }
    void AddFindRequest(const std::string& raw_query, DocumentStatus status);

    void AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        std::vector <Document> documents;
        
        QueryResult(std::vector<Document> doc)
        : documents (doc)
        {
        }
    };
    int requests_on_board_ = 0;
    std::deque<QueryResult> requests_;
    std::deque<QueryResult> requests_successful_;
     const SearchServer& search_server;
    const static int min_in_day_ = 1440;
    // возможно, здесь вам понадобится что-то ещё

    void PushDocument(const std::vector<Document>& result);
};