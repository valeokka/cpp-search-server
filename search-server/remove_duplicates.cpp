#include "remove_duplicates.h"
#include <iostream>
#include <set>


void RemoveDuplicates(SearchServer &search_server)
{
    std::map<int, std::set<std::string>> documents;
    for (const int document_id : search_server)
    {
        auto id_words = search_server.GetWordFrequencies(document_id);
        for (auto it = id_words.begin();
             it != id_words.end(); ++it)
        {
            documents[document_id].insert(it->first);
        }
    }
    for (int i = 0; i < documents.size();++i){
        for(std::string str : documents[i] ){
        }
    }
    std::set <int> for_delete;
    for (const int id0 : search_server)
    {
         if(for_delete.count(id0) != 0){
            continue;
         }
        for (const int id1 : search_server)
        {
            if(for_delete.count(id1) != 0 || id1 == id0){
                continue;
            }
            if (documents[id0] == documents[id1]){
                for_delete.insert(id1);
            }
        }
    }
    for (const int id : for_delete){
        std::cout << "Found duplicate document id " << id << "\n";
        search_server.RemoveDocument(id);
    }
    std::cout << std::flush;
}