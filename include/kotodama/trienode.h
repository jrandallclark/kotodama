#ifndef TRIENODE_H
#define TRIENODE_H

#include <map>
#include <string>
#include <vector>
#include "term.h"

class TrieNode
{
public:
    std::map<std::string, TrieNode*> children;
    Term* term;

    TrieNode(Term* t = nullptr) : term(t) {}

    // Destructor to clean up dynamically allocated children
    ~TrieNode();

    // Insert a phrase into the trie
    void insert(Term* term, const std::vector<std::string>& tokens);

    // Remove a phrase from the trie (cleans up empty nodes)
    bool remove(const std::vector<std::string>& tokens);

    Term* findLongestMatch(const std::vector<std::string>& tokens, int startIndex);

private:
    // Helper for recursive removal
    bool removeHelper(const std::vector<std::string>& tokens, size_t index);
};

#endif // TRIENODE_H
