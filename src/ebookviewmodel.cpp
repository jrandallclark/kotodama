#include "kotodama/ebookviewmodel.h"
#include "kotodama/languageconfig.h"
#include "kotodama/languagemanager.h"
#include "kotodama/tokenizer.h"
#include "kotodama/tokenizerbackend.h"
#include "kotodama/trienode.h"
#include "kotodama/termmanager.h"

#include <QSet>

EbookViewModel::EbookViewModel()
{
}

EbookViewModel::~EbookViewModel()
{
}

void EbookViewModel::loadContent(const QString& text, const QString& language)
{
    this->text = text;
    this->language = language;
    analyzeText();
}

void EbookViewModel::setLanguage(const QString& language)
{
    this->language = language;
}

void EbookViewModel::analyzeText()
{
    tokenizeText();
    findTermMatches();
}

void EbookViewModel::tokenizeText()
{
    tokenBoundaries.clear();

    if (text.isEmpty()) {
        return;
    }

    LanguageConfig config = LanguageManager::instance().getLanguageByCode(language);
    const Tokenizer* tokenizer = config.tokenizer();

    for (const TokenResult& result : tokenizer->tokenize(text)) {
        TokenInfo tokenInfo;
        tokenInfo.startPos = result.startPos;
        tokenInfo.endPos   = result.endPos;
        tokenInfo.text     = QString::fromStdString(result.text);
        tokenBoundaries.push_back(tokenInfo);
    }
}

void EbookViewModel::findTermMatches()
{
    termPositions.clear();

    if (text.isEmpty()) {
        return;
    }

    TrieNode* trie = TermManager::instance().getTrieForLanguage(language);

    // Trie matching uses the same context-free char-based tokenizer as trie building.
    // MeCab (used in tokenizeText) is context-sensitive — it splits the same characters
    // differently depending on surrounding text. Using MeCab tokens here would cause
    // mismatches against the trie built from isolated-term tokenization.
    QString regexStr = LanguageManager::instance().getWordRegex(language);
    bool charBased = LanguageManager::instance().isCharacterBased(language);
    auto matchTok = Tokenizer::createRegex(regexStr, charBased);
    auto matchResults = matchTok->tokenize(text);

    std::vector<std::string> tokens;
    tokens.reserve(matchResults.size());
    for (const auto& r : matchResults) tokens.push_back(r.text);

    for (int i = 0; i < static_cast<int>(tokens.size()); ) {
        Term* matchedTerm = trie->findLongestMatch(tokens, i);

        if (matchedTerm) {
            int startPos = matchResults[i].startPos;
            int endPos = matchResults[i + matchedTerm->tokenCount - 1].endPos;

            TermPosition termPos;
            termPos.startPos = startPos;
            termPos.endPos = endPos;
            termPos.termData = *matchedTerm;
            termPositions.push_back(termPos);

            i += matchedTerm->tokenCount;
        } else {
            i++;
        }
    }
}

std::vector<HighlightInfo> EbookViewModel::getHighlights() const
{
    std::vector<HighlightInfo> highlights;

    if (tokenBoundaries.empty()) {
        return highlights;
    }

    // Track which tokens are part of known terms
    std::vector<bool> tokenUsed(tokenBoundaries.size(), false);

    // Mark tokens that are part of known terms
    size_t termIdx = 0;
    for (size_t i = 0; i < tokenBoundaries.size(); ++i) {
        // Check if this token is the start of a term
        while (termIdx < termPositions.size() &&
               termPositions[termIdx].endPos <= tokenBoundaries[i].startPos) {
            termIdx++;
        }

        // A display token is "used" if its START falls within a known term's range.
        // We intentionally don't require endPos <= term.endPos because MeCab can
        // return a morpheme that starts inside the known term but extends past it
        // (e.g. known term 聞こ [4,6), MeCab token 聞こえ [4,7)).  Checking only
        // the start prevents that token from being emitted as an unknown highlight
        // that would visually mask the known term.
        if (termIdx < termPositions.size() &&
            tokenBoundaries[i].startPos >= termPositions[termIdx].startPos &&
            tokenBoundaries[i].startPos <  termPositions[termIdx].endPos) {
            tokenUsed[i] = true;
        }
    }

    // Create highlights for known terms
    for (const TermPosition& termPos : termPositions) {
        HighlightInfo highlight;
        highlight.startPos = termPos.startPos;
        highlight.endPos = termPos.endPos;
        highlight.level = termPos.termData.level;
        highlight.isUnknown = false;
        highlights.push_back(highlight);
    }

    // Create highlights for unknown words
    for (size_t i = 0; i < tokenBoundaries.size(); ++i) {
        if (!tokenUsed[i]) {
            HighlightInfo highlight;
            highlight.startPos = tokenBoundaries[i].startPos;
            highlight.endPos = tokenBoundaries[i].endPos;
            highlight.level = TermLevel::Recognized;  // Doesn't matter for unknown
            highlight.isUnknown = true;
            highlights.push_back(highlight);
        }
    }

    return highlights;
}

TextProgressStats EbookViewModel::calculateProgressStats() const
{
    // Initialize default stats
    TextProgressStats stats;
    stats.totalUniqueWords = 0;
    stats.knownWords = 0;
    stats.newWords = 0;
    stats.percentKnown = 0.0f;

    // Count unique words
    QSet<QString> uniqueWords;
    for (const TokenInfo& token : tokenBoundaries) {
        uniqueWords.insert(token.text.toLower());
    }
    stats.totalUniqueWords = uniqueWords.size();

    if (stats.totalUniqueWords == 0) {
        return stats;
    }

    // Track which tokens are part of known terms
    std::vector<bool> tokenUsed(tokenBoundaries.size(), false);

    // Mark tokens that are part of known terms
    size_t termIdx = 0;
    for (size_t i = 0; i < tokenBoundaries.size(); ++i) {
        // Check if this token is the start of a term
        while (termIdx < termPositions.size() &&
               termPositions[termIdx].endPos <= tokenBoundaries[i].startPos) {
            termIdx++;
        }

        // Same start-position rule as getHighlights() — see comment there.
        if (termIdx < termPositions.size() &&
            tokenBoundaries[i].startPos >= termPositions[termIdx].startPos &&
            tokenBoundaries[i].startPos <  termPositions[termIdx].endPos) {
            tokenUsed[i] = true;
        }
    }

    // Count known unique words
    QSet<QString> knownUniqueWords;
    for (size_t i = 0; i < tokenBoundaries.size(); ++i) {
        if (tokenUsed[i]) {
            knownUniqueWords.insert(tokenBoundaries[i].text.toLower());
        }
    }
    stats.knownWords = knownUniqueWords.size();

    // Calculate new words and percentage
    stats.newWords = stats.totalUniqueWords - stats.knownWords;
    stats.percentKnown = (static_cast<float>(stats.knownWords) / stats.totalUniqueWords) * 100.0f;

    return stats;
}

int EbookViewModel::findTokenAtPosition(int pos) const
{
    for (size_t i = 0; i < tokenBoundaries.size(); ++i) {
        if (pos >= tokenBoundaries[i].startPos && pos < tokenBoundaries[i].endPos) {
            return i;
        }
    }
    return -1;
}

Term* EbookViewModel::findTermAtPosition(int pos)
{
    for (TermPosition& termPos : termPositions) {
        if (pos >= termPos.startPos && pos < termPos.endPos) {
            return &termPos.termData;
        }
    }
    return nullptr;
}

QString EbookViewModel::getTokenSelectionText(int selectionStart, int selectionEnd) const
{
    if (selectionStart >= selectionEnd) {
        return QString();
    }

    // Find which tokens this selection overlaps
    int startTokenIdx = -1;
    int endTokenIdx = -1;

    for (size_t i = 0; i < tokenBoundaries.size(); ++i) {
        // Token overlaps with selection start
        if (selectionStart >= tokenBoundaries[i].startPos &&
            selectionStart < tokenBoundaries[i].endPos) {
            startTokenIdx = i;
        }
        // Token overlaps with selection end
        if (selectionEnd > tokenBoundaries[i].startPos &&
            selectionEnd <= tokenBoundaries[i].endPos) {
            endTokenIdx = i;
        }
    }

    if (startTokenIdx == -1 || endTokenIdx == -1) {
        return QString();
    }

    // Build the text from complete tokens
    QStringList tokenTexts;
    for (int i = startTokenIdx; i <= endTokenIdx; ++i) {
        tokenTexts.append(tokenBoundaries[i].text);
    }

    return tokenTexts.join(" ");
}

void EbookViewModel::refresh()
{
    analyzeText();
}

void EbookViewModel::refreshTermMatches()
{
    // Only re-match terms without re-tokenizing
    // This is much faster when only terms have changed, not the text
    findTermMatches();
}
