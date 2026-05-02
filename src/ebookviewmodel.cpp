#include "kotodama/ebookviewmodel.h"
#include "kotodama/languageconfig.h"
#include "kotodama/languagemanager.h"
#include "kotodama/tokenizer.h"
#include "kotodama/tokenizerbackend.h"
#include "kotodama/trienode.h"
#include "kotodama/termmanager.h"

#include <QDebug>
#include <QRegularExpression>
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
    focusedTokenIndex = -1;  // Reset focus when tokens are rebuilt

    if (text.isEmpty()) {
        return;
    }

    LanguageConfig config = LanguageManager::instance().getLanguageByCode(language);
    const Tokenizer* tokenizer = config.tokenizer();

    for (const TokenResult& result : tokenizer->tokenize(text)) {
        QString tokenText = QString::fromStdString(result.text);

        bool hasLetter = false;
        for (const QChar& ch : tokenText) {
            if (ch.isLetter()) {
                hasLetter = true;
                break;
            }
        }
        if (!hasLetter) continue;

        // For character-based languages, also filter tokens that contain
        // only non-target-script characters (e.g. Latin letters in Japanese text)
        if (config.isCharBased()) {
            QRegularExpression scriptRe(config.wordRegex());
            if (!tokenText.contains(scriptRe)) continue;
        }

        TokenInfo tokenInfo;
        tokenInfo.startPos = result.startPos;
        tokenInfo.endPos   = result.endPos;
        tokenInfo.text     = tokenText;
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

bool EbookViewModel::setFocusedTokenIndex(int index)
{
    if (index >= -1 && index < static_cast<int>(tokenBoundaries.size())) {
        focusedTokenIndex = index;
        return true;
    }
    qWarning() << "EbookViewModel::setFocusedTokenIndex: Invalid index" << index
               << "- valid range is [-1," << static_cast<int>(tokenBoundaries.size()) - 1 << "]";
    return false;
}

const TokenInfo* EbookViewModel::getFocusedToken() const
{
    if (focusedTokenIndex >= 0 && focusedTokenIndex < static_cast<int>(tokenBoundaries.size())) {
        return &tokenBoundaries[focusedTokenIndex];
    }
    return nullptr;
}

bool EbookViewModel::moveFocusNext()
{
    if (tokenBoundaries.empty()) {
        return false;
    }

    if (focusedTokenIndex < 0) {
        // Start at the first token
        focusedTokenIndex = 0;
        return true;
    }

    if (focusedTokenIndex < static_cast<int>(tokenBoundaries.size()) - 1) {
        focusedTokenIndex++;
        return true;
    }

    // Already at the end
    return false;
}

bool EbookViewModel::moveFocusPrevious()
{
    if (tokenBoundaries.empty()) {
        return false;
    }

    if (focusedTokenIndex < 0) {
        // Start at the last token
        focusedTokenIndex = static_cast<int>(tokenBoundaries.size()) - 1;
        return true;
    }

    if (focusedTokenIndex > 0) {
        focusedTokenIndex--;
        return true;
    }

    // Already at the beginning
    return false;
}

TermPreview EbookViewModel::getPreviewForToken(const TokenInfo* token) const
{
    if (!token) {
        return TermPreview{};
    }

    // Find term at this position
    for (const TermPosition& termPos : termPositions) {
        if (token->startPos >= termPos.startPos && token->startPos < termPos.endPos) {
            return TermPreview{
                termPos.termData.term,
                termPos.termData.pronunciation,
                termPos.termData.definition,
                true
            };
        }
    }

    // Unknown term
    return TermPreview{
        token->text,
        "",
        "Click to add definition",
        false
    };
}

TermValidationResult EbookViewModel::validateTermLength(const QString& cleanedText) const
{
    LanguageConfig config = LanguageManager::instance().getLanguageByCode(language);
    int maxSize = config.tokenLimit();

    if (config.isCharBased()) {
        if (cleanedText.length() > maxSize) {
            return TermValidationResult{
                false,
                QString("Term too long. Maximum %1 characters allowed.").arg(maxSize)
            };
        }
    } else {
        auto tokenResults = config.tokenizer()->tokenize(cleanedText);
        if (static_cast<int>(tokenResults.size()) > maxSize) {
            return TermValidationResult{
                false,
                QString("Term too long. Maximum %1 words allowed.").arg(maxSize)
            };
        }
    }

    return TermValidationResult{true, ""};
}

EditRequest EbookViewModel::getEditRequestForPosition(int position) const
{
    EditRequest request;
    request.language = language;
    request.showWarning = false;
    request.exists = false;

    // Find term at this position
    for (const TermPosition& termPos : termPositions) {
        if (position >= termPos.startPos && position < termPos.endPos) {
            if (termPos.termData.id > 0) {
                request.termText = termPos.termData.term;
                request.exists = true;
                request.existingPronunciation = termPos.termData.pronunciation;
                request.existingDefinition = termPos.termData.definition;
                request.existingLevel = termPos.termData.level;
                break;
            }
        }
    }

    // No term found - use token at position
    if (!request.exists) {
        int tokenIdx = findTokenAtPosition(position);
        if (tokenIdx >= 0 && tokenIdx < static_cast<int>(tokenBoundaries.size())) {
            request.termText = tokenBoundaries[tokenIdx].text;
        } else {
            // Invalid position - return empty request (View will handle as "no selection")
            request.termText = "";
            return request;
        }
    }

    // Clean the text
    CleanedText cleaned = TermManager::cleanTermText(request.termText, language);
    if (cleaned.cleaned.isEmpty()) {
        request.showWarning = true;
        request.warningMessage = "The selection contains no valid text.";
        return request;
    }

    request.termText = cleaned.cleaned;

    // Validate term length
    TermValidationResult validation = validateTermLength(request.termText);
    if (!validation.isValid) {
        request.showWarning = true;
        request.warningMessage = validation.warningMessage;
        return request;
    }

    // Check existence in database (for new terms that were matched from trie but not yet saved)
    if (!request.exists) {
        request.exists = TermManager::instance().termExists(request.termText, language);
        if (request.exists) {
            Term existingTerm = TermManager::instance().getTerm(request.termText, language);
            request.existingPronunciation = existingTerm.pronunciation;
            request.existingDefinition = existingTerm.definition;
            request.existingLevel = existingTerm.level;
        }
    }

    return request;
}

EditRequest EbookViewModel::getEditRequestForSelection(int selectionStart, int selectionEnd) const
{
    // First check if click was inside a multi-word term
    EditRequest request;
    request.language = language;
    request.showWarning = false;
    request.exists = false;

    // Check for multi-word term at selection start
    for (const TermPosition& termPos : termPositions) {
        if (selectionStart >= termPos.startPos && selectionStart < termPos.endPos) {
            if (termPos.termData.tokenCount > 1) {
                // Multi-word term - use the full term
                request.termText = termPos.termData.term;
                request.exists = true;
                request.existingPronunciation = termPos.termData.pronunciation;
                request.existingDefinition = termPos.termData.definition;
                request.existingLevel = termPos.termData.level;
                break;
            }
        }
    }

    // If no multi-word term, use token-snapped selection
    if (!request.exists) {
        QString selectedText = getTokenSelectionText(selectionStart, selectionEnd);
        if (!selectedText.isEmpty()) {
            // Check if this text corresponds to a known term
            for (const TermPosition& termPos : termPositions) {
                if (selectedText == termPos.termData.term) {
                    request.termText = termPos.termData.term;
                    request.exists = true;
                    request.existingPronunciation = termPos.termData.pronunciation;
                    request.existingDefinition = termPos.termData.definition;
                    request.existingLevel = termPos.termData.level;
                    break;
                }
            }
            if (!request.exists) {
                request.termText = selectedText;
            }
        }
    }

    // If still no text, return empty request
    if (request.termText.isEmpty()) {
        return request;
    }

    // Clean the text
    CleanedText cleaned = TermManager::cleanTermText(request.termText, language);
    if (cleaned.cleaned.isEmpty()) {
        request.showWarning = true;
        request.warningMessage = "The selection contains no valid text.";
        return request;
    }

    request.termText = cleaned.cleaned;

    // Validate term length
    TermValidationResult validation = validateTermLength(request.termText);
    if (!validation.isValid) {
        request.showWarning = true;
        request.warningMessage = validation.warningMessage;
        return request;
    }

    // Check existence if not already known
    if (!request.exists) {
        request.exists = TermManager::instance().termExists(request.termText, language);
        if (request.exists) {
            Term existingTerm = TermManager::instance().getTerm(request.termText, language);
            request.existingPronunciation = existingTerm.pronunciation;
            request.existingDefinition = existingTerm.definition;
            request.existingLevel = existingTerm.level;
        }
    }

    return request;
}

EditRequest EbookViewModel::getEditRequestForFocusedToken() const
{
    const TokenInfo* token = getFocusedToken();
    if (!token) {
        EditRequest emptyRequest;
        emptyRequest.language = language;
        emptyRequest.showWarning = false;
        emptyRequest.exists = false;
        emptyRequest.termText = "";
        return emptyRequest;
    }

    return getEditRequestForPosition(token->startPos);
}

QPair<int, int> EbookViewModel::getFocusRange(int tokenIndex) const
{
    if (tokenIndex < 0 || tokenIndex >= static_cast<int>(tokenBoundaries.size())) {
        return qMakePair(-1, -1);
    }

    const TokenInfo& token = tokenBoundaries[tokenIndex];

    // Check if this token is part of a known term
    for (const TermPosition& termPos : termPositions) {
        if (token.startPos >= termPos.startPos && token.startPos < termPos.endPos) {
            // Token is inside a known term - return the full term span
            return qMakePair(termPos.startPos, termPos.endPos);
        }
    }

    // Not part of a known term - return the token's own span
    return qMakePair(token.startPos, token.endPos);
}

int EbookViewModel::findFirstTokenAtOrAfter(int pos) const
{
    for (size_t i = 0; i < tokenBoundaries.size(); ++i) {
        // Token is at/after pos if it contains pos or starts after pos
        if (tokenBoundaries[i].endPos > pos) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int EbookViewModel::findLastTokenAtOrBefore(int pos) const
{
    int lastIndex = -1;
    for (size_t i = 0; i < tokenBoundaries.size(); ++i) {
        if (tokenBoundaries[i].startPos <= pos) {
            lastIndex = static_cast<int>(i);
        } else {
            break;  // Tokens are sorted by position
        }
    }
    return lastIndex;
}
