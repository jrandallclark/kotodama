#ifndef TERMMANAGER_H
#define TERMMANAGER_H

#include <QString>
#include <QMap>
#include <QList>
#include <vector>
#include "term.h"
#include "trienode.h"

struct CleanedText {
    QString cleaned;
    QString removed;
};

class TermManager
{
public:
    static TermManager& instance();

    static CleanedText cleanTermText(const QString& text, const QString& language);

    // Get the trie for a language (builds if needed)
    TrieNode* getTrieForLanguage(const QString& language);

    // Add a new term (updates database AND trie)
    bool addTerm(const QString& term,
                 const QString& language,
                 TermLevel level = TermLevel::Recognized,
                 const QString& definition = "",
                 const QString& pronunciation = "");

    // Update an existing term (updates database AND trie)
    bool updateTerm(const QString& term,
                    const QString& language,
                    TermLevel level,
                    const QString& definition,
                    const QString& pronunciation);

    // Update just the level (quick operation for status changes)
    bool updateTermLevel(const QString& term, const QString& language, TermLevel level);

    // Delete a term (updates database AND trie)
    bool deleteTerm(const QString& term, const QString& language);

    Term getTerm(const QString& term, const QString& language);

    bool termExists(const QString& term, const QString& language);

    // Force reload of trie for a language
    void reloadLanguage(const QString& language);

    void clearCache();

    // Get term count for a language
    int getTermCount(const QString& language);

    struct ImportResult {
        int successCount;
        int failCount;
        QStringList errors;
    };

    struct TermToImport {
        QString term;
        QString language;
        TermLevel level;
        QString definition;
        QString pronunciation;
    };

    ImportResult batchAddTerms(const QList<TermToImport>& terms, bool updateDuplicates = false);

private:
    TermManager();
    ~TermManager();
    TermManager(const TermManager&) = delete;
    TermManager& operator=(const TermManager&) = delete;

    QMap<QString, TrieNode*> tries;
    QMap<QString, std::vector<Term*>> terms;

    void buildTrieForLanguage(const QString& language);
    void clearLanguageData(const QString& language);

    // Incremental trie updates (much faster than full rebuild)
    void addTermToTrie(const QString& term, const QString& language, TermLevel level,
                       const QString& definition, const QString& pronunciation);
    void removeTermFromTrie(const QString& term, const QString& language);
    void updateTermInTrie(const QString& term, const QString& language, TermLevel level,
                          const QString& definition, const QString& pronunciation);
};

#endif // TERMMANAGER_H
