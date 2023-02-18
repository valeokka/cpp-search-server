#include "remove_duplicates.h"
#include <iostream>
#include <set>


void RemoveDuplicates(SearchServer &search_server)
{
    std::map<std::set<std::string>, std::set<int>> documents;
    for (const int document_id : search_server)
    {
        //В SearchServer существует map со словами и их частотой. Достаем из map только слова и добавляем во множество 
        std::set <std::string> words_id;
        auto id_words = search_server.GetWordFrequencies(document_id);
        for (auto it = id_words.begin();
        
             it != id_words.end(); ++it)
        {
            words_id.insert(it->first);
        }
        documents[words_id].insert(document_id);
    }

    std::set <int> for_delete;
    for (auto it = documents.begin(); it != documents.end(); ++it)
    {
        if (it->second.size() > 1){
            for (auto it1 = it->second.begin(); it1 != it->second.end(); ++it1){
                if (it1 == it->second.begin()){
                    continue;
                }
                for_delete.insert(*it1);
            }
        }
        
    }
    for (const int id : for_delete){
        std::cout << "Found duplicate document id " << id << "\n";
        search_server.RemoveDocument(id);
    }
    std::cout << std::flush;
}