#include <iostream>
#include <vector>
#include "paginator.h"
#include "document.h"
#include "search_server.h"
#include "request_queue.h"
#include "read_input_functions.h"
#include "paginator.h"
#include "log_duration.h"
#include "remove_duplicates.h"

    using namespace std::string_literals;

using namespace std;
template <typename It>
std::ostream& operator<<(std::ostream& out, const IteratorRange<It>& p){
    for (auto i = p.begin; i < p.end; i++) {
        out << *i;
    }
    return out;
}
std::ostream& operator << (std::ostream& out, const Document search){
    return out << "{ document_id = " << search.id << ", relevance = " << search.relevance << ", rating = " << search.rating << " }";
 }

int main() {
    SearchServer search_server("and with"s);

    search_server.AddDocument( 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument( 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // дубликат документа 2, будет удалён
    search_server.AddDocument( 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // отличие только в стоп-словах, считаем дубликатом
    search_server.AddDocument( 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, считаем дубликатом документа 1
    search_server.AddDocument( 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // добавились новые слова, дубликатом не является
    search_server.AddDocument( 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    search_server.AddDocument( 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, {1, 2});

    // есть не все слова, не является дубликатом
    search_server.AddDocument( 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, {1, 2});

    // слова из разных документов, не является дубликатом
    search_server.AddDocument( 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, {1, 2});
    
    cout << "Before duplicates removed: "s << search_server.GetDocumentCount() << endl;
    RemoveDuplicates(search_server);
    cout << "After duplicates removed: "s << search_server.GetDocumentCount() << endl;
} 