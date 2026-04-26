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

// Data for term preview display
struct TermPreview {
    QString term;
    QString pronunciation;
    QString definition;
    bool isKnown;
};

// Validation result for term length checking
struct TermValidationResult {
    bool isValid;
    QString warningMessage;  // empty if valid
};

// Data for edit panel request
struct EditRequest {
    QString termText;
    QString language;
    bool exists;
    QString existingPronunciation;
    QString existingDefinition;
    TermLevel existingLevel;
    bool showWarning;
    QString warningMessage;
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

    // Focus for keyboard navigation
    bool setFocusedTokenIndex(int index);  // Returns true if set, false if out-of-range
    int getFocusedTokenIndex() const { return focusedTokenIndex; }
    const TokenInfo* getFocusedToken() const;
    bool moveFocusNext();      // Returns true if moved
    bool moveFocusPrevious();  // Returns true if moved

    // Preview data for display
    TermPreview getPreviewForToken(const TokenInfo* token) const;

    // Validation and edit request
    TermValidationResult validateTermLength(const QString& cleanedText) const;
    EditRequest getEditRequestForPosition(int position) const;
    EditRequest getEditRequestForSelection(int selectionStart, int selectionEnd) const;
    EditRequest getEditRequestForFocusedToken() const;

    // Focus highlighting (shared logic with base highlights)
    // Returns the range to highlight for focus; expands to full term span
    // if the token is part of a known multi-token term
    QPair<int, int> getFocusRange(int tokenIndex) const;

    // Find first token at or after a text position (for viewport-aware focus)
    int findFirstTokenAtOrAfter(int pos) const;
    // Find last token at or before a text position
    int findLastTokenAtOrBefore(int pos) const;

private:
    QString text;
    QString language;
    std::vector<TokenInfo> tokenBoundaries;
    std::vector<TermPosition> termPositions;
    int focusedTokenIndex = -1;  // -1 means no focus

    // Helper methods
    void tokenizeText();
    void findTermMatches();
};

#endif // EBOOKVIEWMODEL_H
