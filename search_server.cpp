#include "search_server.h"
#include "iterator_range.h"
#include "profile.h"
#include <algorithm>
#include <iterator>
#include <sstream>
#include "parse.h"
#include <iostream>
#include <array>
#include<utility>
#include<functional>
#include<numeric>
vector<string> SplitIntoWords(const string& line) {
  istringstream words_input(line);
  return {istream_iterator<string>(words_input), istream_iterator<string>()};
}

InvertedIndex::InvertedIndex(istream& document_input) {
    for (string current_document; getline(document_input, current_document); ) {
        docs.push_back(move(current_document));
        size_t docid = docs.size() - 1;
        for (string_view word : SplitIntoWordsView(docs.back())) {
            auto& docids = index[word];
            if (!docids.empty() && docids.back().docid == docid) {
                ++docids.back().hitcount;
            }
            else {
                docids.push_back({ docid, 1 });
            }
        }
    }
}
void UpdateDocumentBaseOneThread(istream& document_input, Synchronized<InvertedIndex>& index) {
    InvertedIndex new_index(document_input);
    swap(index.GetAccess().ref_to_value, new_index);
}

void SearchServer::UpdateDocumentBase(istream& document_input) {
    futures.push_back(async(UpdateDocumentBaseOneThread, ref(document_input), ref(index)));
}
void AddQueriesStreamOneThread(istream& query_input, ostream& search_results_output, Synchronized<InvertedIndex>& index) {
    vector<size_t> docid_count;
    vector<int> search_results;
    for (string current_query; getline(query_input, current_query); ) {
        const auto words = SplitIntoWords(current_query);
        {
            auto new_index = index.GetAccess();
            docid_count.assign(new_index.ref_to_value.GetDocument().size(), 0);
            search_results.resize(new_index.ref_to_value.GetDocument().size());

            for (const auto& word : words) {
                for (const auto& [docid, hit_count] : new_index.ref_to_value.Lookup(word)) {
                    docid_count[docid] += hit_count;
                }
            }
        }
        iota(search_results.begin(), search_results.end(), 0);
        {
            partial_sort(search_results.begin(), Head(search_results, 5).end(), search_results.end(), [&docid_count](int lhs, int rhs) {
                return make_pair(docid_count[lhs], -lhs) > make_pair(docid_count[rhs], -rhs); });
        }
        search_results_output << current_query << ':';
        for (auto docid : Head(search_results, 5)) {
            if (docid_count[docid] == 0) {
                break;
            }
            search_results_output << " {"
                << "docid: " << docid << ", "
                << "hitcount: " << docid_count[docid] << '}';
        }
        search_results_output << '\n';
    }
}


void SearchServer::AddQueriesStream(
    istream& query_input, ostream& search_results_output) {
    futures.push_back(async(AddQueriesStreamOneThread, ref(query_input), ref(search_results_output), ref(index)));
}

const vector<InvertedIndex::Entry>& InvertedIndex::Lookup(string_view word) const {
    static const vector<Entry> empty;
    if (auto it = index.find(word); it != index.end()) {
        return it->second;
    }
    else {
        return empty;
    }
}