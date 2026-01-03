#include "kotodama/trienode.h"
#include "kotodama/term.h"
#include <QString>
#include <QtCore/qdebug.h>
#include <iostream>

TrieNode::~TrieNode() {
    // Clean up all child nodes
    for (auto& pair : children) {
        delete pair.second;
    }
}

void TrieNode::insert(Term* term, const std::vector<std::string>& tokens) {
    TrieNode* current = this;

    for (size_t i = 0; i < tokens.size(); i++) {
        // Use QString for proper Unicode lowercasing
        QString qToken = QString::fromStdString(tokens[i]);
        QString qLowerToken = qToken.toLower();
        std::string lowerToken = qLowerToken.toStdString();

        if (current->children.find(lowerToken) == current->children.end()) {
            current->children[lowerToken] = new TrieNode();
        }

        current = current->children[lowerToken];
    }

    current->term = term;
}

bool TrieNode::remove(const std::vector<std::string>& tokens) {
    return removeHelper(tokens, 0);
}

bool TrieNode::removeHelper(const std::vector<std::string>& tokens, size_t index) {
    // Base case: we've reached the end of the token sequence
    if (index >= tokens.size()) {
        if (term != nullptr) {
            term = nullptr;
            // Return true if this node should be deleted (no children, no term)
            return children.empty();
        }
        return false;
    }

    // Get the lowercase version of the current token
    QString qToken = QString::fromStdString(tokens[index]);
    QString qLowerToken = qToken.toLower();
    std::string lowerToken = qLowerToken.toStdString();

    // Find the child node for this token
    auto it = children.find(lowerToken);
    if (it == children.end()) {
        // Token not found - nothing to remove
        return false;
    }

    // Recursively remove from the child
    bool shouldDeleteChild = it->second->removeHelper(tokens, index + 1);

    if (shouldDeleteChild) {
        // Delete the child node and remove from map
        delete it->second;
        children.erase(it);
    }

    // Return true if this node should be deleted
    // (has no children and no term)
    return children.empty() && term == nullptr;
}

Term* TrieNode::findLongestMatch(const std::vector<std::string>& tokens, int startIndex) {
    TrieNode* current = this;
    TrieNode* longestMatchNode = nullptr;

    for (int i = startIndex; i < tokens.size(); i++) {
        // Use QString for proper Unicode lowercasing
        QString qToken = QString::fromStdString(tokens[i]);
        QString qLowerToken = qToken.toLower();
        std::string lowerToken = qLowerToken.toStdString();

        auto it = current->children.find(lowerToken);
        if (it == current->children.end()) {
            break;
        }

        current = it->second;

        if (current->term) {
            longestMatchNode = current;
        }
    }

    if (longestMatchNode) {
        return longestMatchNode->term;
    }
    return nullptr;
}


