#ifndef TRIENODE_H
#define TRIENODE_H

#include <string>
#include <unordered_map>
#include <vector>
#include "term.h"

// Normalize a tokenizer-emitted token to its trie-key form (Unicode lowercase).
// Fast-path: bytes already pure-ASCII-lowercase are returned untouched.
std::string toLowerTrieKey(const std::string& token);

class TrieNode
{
public:
    std::unordered_map<std::string, TrieNode*> children;
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
