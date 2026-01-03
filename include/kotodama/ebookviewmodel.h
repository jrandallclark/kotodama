#ifndef EBOOKVIEWMODEL_H
#define EBOOKVIEWMODEL_H

#include "term.h"
#include "progresscalculator.h"
#include <QString>
#include <vector>

class TrieNode;
class QRegularExpression;

// Data structures for model state
struct TokenInfo {
    int startPos;
    int endPos;
    QString text;
};

struct TermPosition {
    int startPos;
    int endPos;
    Term termData;
};

struct HighlightInfo {
    int startPos;
    int endPos;
    TermLevel level;
    bool isUnknown;  // True for unknown words (gray highlighting)
};

struct SelectionInfo {
    QString text;
    int startPos;
    int endPos;
    Term* existingTerm;  // nullptr if not found
};

// Business logic for ebook text analysis and term matching
class EbookViewModel
{
public:
    EbookViewModel();
    ~EbookViewModel();

    // Load content
    void loadContent(const QString& text, const QString& language);
    void setLanguage(const QString& language);

    // Tokenization and analysis
    void analyzeText();
    std::vector<HighlightInfo> getHighlights() const;
    TextProgressStats calculateProgressStats() const;

    // Selection and position queries
    int findTokenAtPosition(int pos) const;
    Term* findTermAtPosition(int pos);
    QString getTokenSelectionText(int selectionStart, int selectionEnd) const;

    // Getters
    QString getText() const { return text; }
    QString getLanguage() const { return language; }
    const std::vector<TokenInfo>& getTokenBoundaries() const { return tokenBoundaries; }
    const std::vector<TermPosition>& getTermPositions() const { return termPositions; }

    // Trigger re-analysis (e.g., after terms change)
    void refresh();
    void refreshTermMatches();  // Only re-match terms, don't re-tokenize

private:
    QString text;
    QString language;
    std::vector<TokenInfo> tokenBoundaries;
    std::vector<TermPosition> termPositions;

    // Helper methods
    void tokenizeText();
    void findTermMatches();
};

#endif // EBOOKVIEWMODEL_H
