#ifndef EBOOKVIEWMODEL_H
#define EBOOKVIEWMODEL_H

#include "term.h"
#include "progresscalculator.h"
#include "kotodama/tokenizerbackend.h"
#include "kotodama/ilanguageprovider.h"
#include "kotodama/itermstore.h"
#include <QPair>
#include <QString>
#include <optional>
#include <unordered_map>
#include <vector>

class TrieNode;
class QRegularExpression;

// Data structures for model state
struct TokenInfo {
    int startPos;
    int endPos;
    QString text;
    std::optional<TermLevel> level;
    bool hasTerm() const { return level.has_value(); }
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
    EbookViewModel(ILanguageProvider* langProvider = nullptr,
                   ITermStore* termStore = nullptr);
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
    const Term* findTermAtPosition(int pos);
    QString getTokenSelectionText(int selectionStart, int selectionEnd) const;

    // Getters
    QString getText() const { return m_text; }
    QString getLanguage() const { return m_language; }
    const std::vector<TokenInfo>& getTokenBoundaries() const { return m_tokenBoundaries; }
    const std::vector<TermPosition>& getTermPositions() const { return m_termPositions; }

    // Trigger re-analysis (e.g., after terms change)
    void refresh();
    void refreshTermMatches();  // Only re-match terms, don't re-tokenize

    // Incremental term-position updates — return changed (start,end) ranges
    // for the viewer to re-highlight. O(occurrences-of-term), no full rescan.
    std::vector<QPair<int,int>> addTermPositions(const Term& term);
    std::vector<QPair<int,int>> removeTermPositions(const QString& termText);

    // Rebuild steps (public for yielding scheduler in viewer)
    void findTermMatches();
    void buildDisplayTokens();

    // Chunked term matching — builds results incrementally so the viewer
    // can yield to the event loop between chunks, keeping the UI responsive.
    void beginChunkedTermMatching();
    // Returns true when all tokens have been processed.
    bool processMatchChunk(int maxTokens);
    void commitTermMatches();

    // Focus for keyboard navigation
    bool setFocusedTokenIndex(int index);  // Returns true if set, false if out-of-range
    int getFocusedTokenIndex() const { return m_focusedTokenIndex; }
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
    // Dependency injection — default adapters wrap singletons, can be overridden for testing
    ILanguageProvider* m_langProvider;
    ITermStore* m_termStore;

    QString m_text;
    QString m_language;
    std::vector<TokenInfo> m_rawDisplayTokens;  // raw MeCab/regex output (before merge)
    std::vector<TokenInfo> m_tokenBoundaries;
    std::vector<TermPosition> m_termPositions;
    std::unordered_map<int, size_t> m_termIdxByStartPos;  // startPos → index in m_termPositions
    int m_focusedTokenIndex = -1;

    // Cached trie tokens — rebuilt only on load, reused on term add/delete
    std::vector<TokenResult> m_cachedMatchResults;
    std::vector<std::string> m_cachedTrieTokens;

    // Inverted index: first-token → list of position indices in m_cachedTrieTokens.
    // Built lazily with the cache; used by addTermPositions for O(occurrences) lookup.
    std::unordered_map<std::string, std::vector<int>> m_trieTokenIdxByFirstToken;

    // Chunked term matching state
    std::vector<TermPosition> m_pendingTermPositions;
    int m_chunkScanIndex = 0;
    int m_chunkTotalTokens = 0;

    // Helper methods
    void tokenizeText();
    void indexTermPositions();
    const TermPosition* findTermPosition(const TokenInfo& token) const;
};

#endif // EBOOKVIEWMODEL_H
