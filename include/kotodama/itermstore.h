#ifndef ITERMSTORE_H
#define ITERMSTORE_H

#include "term.h"
#include <QString>

class TrieNode;

struct ITermStore {
    virtual ~ITermStore() = default;
    virtual TrieNode* getTrieForLanguage(const QString&) = 0;
    virtual bool      termExists(const QString&, const QString&) const = 0;
    virtual Term      getTerm(const QString&, const QString&) const = 0;
};

#endif // ITERMSTORE_H
