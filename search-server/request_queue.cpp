//Вставьте сюда своё решение из урока «Очередь запросов» темы «Стек, очередь, дек».‎#pragma one
#include <string>
#include <vector>
#include <deque>
#include "search_server.h"
#include "request_queue.h"

void RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
        std::vector <Document> result = search_server.FindTopDocuments(raw_query, status);
        PushDocument(result);
}
void RequestQueue::AddFindRequest(const std::string& raw_query) {
    std::vector <Document> result = search_server.FindTopDocuments(raw_query);
    PushDocument(result);
}
int RequestQueue::GetNoResultRequests() const {
     return requests_.size();
}

void RequestQueue::PushDocument(const std::vector<Document>& result){
if (result.empty()){
    requests_.push_back(result);
}
else{
    requests_successful_.push_back(result);
}
++requests_on_board_;
if(requests_on_board_ == min_in_day_ +1){
    requests_.pop_front();
    --requests_on_board_;
}
}