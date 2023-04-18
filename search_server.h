#pragma once
#include<mutex>
#include <istream>
#include <ostream>
#include <set>
#include <list>
#include <vector>
#include <map>
#include<future>
#include <string>
#include<deque>
#include"synchronized.h"
using namespace std;


class InvertedIndex {
public:
    struct Entry {
        size_t docid, hitcount;
    };
    InvertedIndex() = default;
    explicit InvertedIndex(istream& document_input);
    const vector<Entry>& Lookup(string_view word) const;

  const deque<string>& GetDocument() const {
      return docs;
  }

private:
  map<string_view, vector<Entry>> index;
  deque<string> docs;
};

class SearchServer {
public:
  SearchServer() = default;
  explicit SearchServer(istream& document_input)
      : index(InvertedIndex(document_input))
  {
  }
  void UpdateDocumentBase(istream& document_input);
  void AddQueriesStream(istream& query_input, ostream& search_results_output);

private:
  Synchronized<InvertedIndex> index;
  vector<future<void>> futures;
};
void UpdateDocumentBaseOneThread(istream& document_input, Synchronized<InvertedIndex>& index);
void AddQueriesStreamOneThread(istream& query_input, ostream& search_results_output, Synchronized<InvertedIndex>& index);