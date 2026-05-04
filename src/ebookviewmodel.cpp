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
#include <algorithm>
#include <climits>
#include <unordered_map>

// Default adapters that delegate to singleton managers (used for DI injection)
struct LanguageManagerAdapter : ILanguageProvider {
    LanguageConfig getLanguageByCode(const QString& code) const override {
        return LanguageManager::instance().getLanguageByCode(code);
    }
    QString getWordRegex(const QString& code) const override {
        return LanguageManager::instance().getWordRegex(code);
    }
    bool isCharacterBased(const QString& code) const override {
        return LanguageManager::instance().isCharacterBased(code);
    }
};

struct TermManagerAdapter : ITermStore {
    TrieNode* getTrieForLanguage(const QString& language) override {
        return TermManager::instance().getTrieForLanguage(language);
    }
    bool termExists(const QString& term, const QString& language) const override {
        return TermManager::instance().termExists(term, language);
    }
    Term getTerm(const QString& term, const QString& language) const override {
        return TermManager::instance().getTerm(term, language);
    }
};

EbookViewModel::EbookViewModel(ILanguageProvider* langProvider, ITermStore* termStore)
{
    if (!langProvider) {
        static LanguageManagerAdapter defaultLangProvider;
        m_langProvider = &defaultLangProvider;
    } else {
        m_langProvider = langProvider;
    }
    if (!termStore) {
        static TermManagerAdapter defaultTermStore;
        m_termStore = &defaultTermStore;
    } else {
        m_termStore = termStore;
    }
}

EbookViewModel::~EbookViewModel()
{
}

void EbookViewModel::loadContent(const QString& text, const QString& language)
{
    this->m_text = text;
    this->m_language = language;
    m_cachedMatchResults.clear();
    m_cachedTrieTokens.clear();
    m_trieTokenIdxByFirstToken.clear();
    analyzeText();
}

void EbookViewModel::setLanguage(const QString& language)
{
    this->m_language = language;
}

void EbookViewModel::analyzeText()
{
    findTermMatches();
    tokenizeText();
    buildDisplayTokens();
}

void EbookViewModel::tokenizeText()
{
    m_rawDisplayTokens.clear();
    // Don't clear m_cachedMatchResults/m_cachedTrieTokens here — they're
    // built once in findTermMatches() and reused across chunked rebuilds.
    // loadContent() clears them when loading new text.

    if (m_text.isEmpty()) {
        return;
    }

    LanguageConfig config = m_langProvider->getLanguageByCode(m_language);
    const Tokenizer* tokenizer = config.tokenizer();

    QRegularExpression scriptRe;
    if (config.isCharBased()) {
        scriptRe = QRegularExpression(config.wordRegex());
    }

    for (const TokenResult& result : tokenizer->tokenize(m_text)) {
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
        if (scriptRe.isValid() && !tokenText.contains(scriptRe)) continue;

        TokenInfo tokenInfo;
        tokenInfo.startPos = result.startPos;
        tokenInfo.endPos   = result.endPos;
        tokenInfo.text     = tokenText;
        m_rawDisplayTokens.push_back(tokenInfo);
    }
}

void EbookViewModel::findTermMatches()
{
    m_termPositions.clear();

    if (m_text.isEmpty()) {
        return;
    }

    TrieNode* trie = m_termStore->getTrieForLanguage(m_language);

    // Tokenize only once, cache for re-matches (add/delete term)
    if (m_cachedMatchResults.empty()) {
        QString regexStr = m_langProvider->getWordRegex(m_language);
        bool charBased = m_langProvider->isCharacterBased(m_language);
        auto matchTok = Tokenizer::createRegex(regexStr, charBased);
        m_cachedMatchResults = matchTok->tokenize(m_text);

        m_cachedTrieTokens.clear();
        m_cachedTrieTokens.reserve(m_cachedMatchResults.size());
        for (const auto& r : m_cachedMatchResults) {
            m_cachedTrieTokens.push_back(toLowerTrieKey(r.text));
        }

        m_trieTokenIdxByFirstToken.clear();
        for (int i = 0; i < static_cast<int>(m_cachedTrieTokens.size()); ++i) {
            m_trieTokenIdxByFirstToken[m_cachedTrieTokens[i]].push_back(i);
        }
    }

    const auto& matchResults = m_cachedMatchResults;
    const auto& tokens = m_cachedTrieTokens;

    for (int i = 0; i < static_cast<int>(tokens.size()); ) {
        Term* matchedTerm = trie->findLongestMatch(tokens, i);

        if (matchedTerm) {
            int startPos = matchResults[i].startPos;
            int endPos = matchResults[i + matchedTerm->tokenCount - 1].endPos;

            TermPosition termPos;
            termPos.startPos = startPos;
            termPos.endPos = endPos;
            termPos.termData = *matchedTerm;
            m_termPositions.push_back(termPos);

            i += matchedTerm->tokenCount;
        } else {
            i++;
        }
    }
    indexTermPositions();
}

void EbookViewModel::indexTermPositions()
{
    m_termIdxByStartPos.clear();
    for (size_t i = 0; i < m_termPositions.size(); ++i) {
        m_termIdxByStartPos[m_termPositions[i].startPos] = i;
    }
}

const TermPosition* EbookViewModel::findTermPosition(const TokenInfo& token) const
{
    auto it = m_termIdxByStartPos.find(token.startPos);
    if (it != m_termIdxByStartPos.end()) {
        const TermPosition& tp = m_termPositions[it->second];
        if (tp.endPos == token.endPos) {
            return &tp;
        }
    }
    return nullptr;
}

void EbookViewModel::buildDisplayTokens()
{
    // Merge raw MeCab tokens and known-term positions into a single
    // display-token layer using longest-span-wins at each character
    // position.  Store known/unknown status directly on each token.

    m_tokenBoundaries.clear();
    m_focusedTokenIndex = -1;

    if (m_text.isEmpty()) return;

    struct Candidate {
        int start;
        int end;
        QString text;
        bool  isTrie;
    };

    std::vector<Candidate> candidates;

    // Raw display tokens (MeCab or char-based fallback)
    for (const TokenInfo& rt : m_rawDisplayTokens) {
        candidates.push_back({rt.startPos, rt.endPos, rt.text, false});
    }

    // Known-term spans (store just positions, extract text only for winners)
    for (const TermPosition& tp : m_termPositions) {
        candidates.push_back({tp.startPos, tp.endPos, QString(), true});
    }

    // Sort: start ASC, length DESC, trie-first
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  if (a.start != b.start) return a.start < b.start;
                  int aLen = a.end - a.start;
                  int bLen = b.end - b.start;
                  if (aLen != bLen) return aLen > bLen;
                  return a.isTrie > b.isTrie;
              });

    // Greedy selection — tokens are naturally emitted in startPos order
    int coveredUntil = 0;
    for (const Candidate& c : candidates) {
        if (c.start < coveredUntil) continue;
        TokenInfo token;
        token.startPos = c.start;
        token.endPos   = c.end;
        token.text = c.isTrie ? m_text.mid(c.start, c.end - c.start) : c.text;

        auto it = m_termIdxByStartPos.find(c.start);
        if (it != m_termIdxByStartPos.end() && m_termPositions[it->second].endPos == c.end) {
            token.level = m_termPositions[it->second].termData.level;
        }

        m_tokenBoundaries.push_back(token);
        coveredUntil = c.end;
    }
}

std::vector<HighlightInfo> EbookViewModel::getHighlights() const
{
    std::vector<HighlightInfo> highlights;
    highlights.reserve(m_tokenBoundaries.size());

    for (const TokenInfo& token : m_tokenBoundaries) {
        HighlightInfo h;
        h.startPos  = token.startPos;
        h.endPos    = token.endPos;
        h.level     = token.level.value_or(TermLevel::Recognized);
        h.isUnknown = !token.hasTerm();
        highlights.push_back(h);
    }

    return highlights;
}

TextProgressStats EbookViewModel::calculateProgressStats() const
{
    TextProgressStats stats;

    QSet<QString> uniqueWords;
    for (const TokenInfo& token : m_tokenBoundaries) {
        uniqueWords.insert(token.text.toLower());
    }
    stats.totalUniqueWords = uniqueWords.size();

    if (stats.totalUniqueWords == 0) {
        return stats;
    }

    QSet<QString> knownUniqueWords;
    for (const TokenInfo& token : m_tokenBoundaries) {
        if (token.hasTerm()) {
            knownUniqueWords.insert(token.text.toLower());
        }
    }
    stats.knownWords = knownUniqueWords.size();

    stats.newWords = stats.totalUniqueWords - stats.knownWords;
    stats.percentKnown = (stats.knownWords * 100) / stats.totalUniqueWords;

    return stats;
}

int EbookViewModel::findTokenAtPosition(int pos) const
{
    // Binary search — m_tokenBoundaries is sorted, non-overlapping
    auto it = std::lower_bound(m_tokenBoundaries.begin(), m_tokenBoundaries.end(), pos,
        [](const TokenInfo& t, int p) { return t.endPos <= p; });
    if (it != m_tokenBoundaries.end() && pos >= it->startPos && pos < it->endPos) {
        return static_cast<int>(it - m_tokenBoundaries.begin());
    }
    return -1;
}

const Term* EbookViewModel::findTermAtPosition(int pos)
{
    int tokenIdx = findTokenAtPosition(pos);
    if (tokenIdx < 0) return nullptr;

    const TermPosition* tp = findTermPosition(m_tokenBoundaries[tokenIdx]);
    return tp ? &tp->termData : nullptr;
}

QString EbookViewModel::getTokenSelectionText(int selectionStart, int selectionEnd) const
{
    if (selectionStart >= selectionEnd) {
        return QString();
    }

    int startTokenIdx = findTokenAtPosition(selectionStart);
    int endTokenIdx = findTokenAtPosition(selectionEnd - 1);

    if (startTokenIdx < 0 || endTokenIdx < 0) {
        return QString();
    }

    // Build the text from complete tokens
    QStringList tokenTexts;
    for (int i = startTokenIdx; i <= endTokenIdx; ++i) {
        tokenTexts.append(m_tokenBoundaries[i].text);
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
    buildDisplayTokens();
}

bool EbookViewModel::setFocusedTokenIndex(int index)
{
    if (index >= -1 && index < static_cast<int>(m_tokenBoundaries.size())) {
        m_focusedTokenIndex = index;
        return true;
    }
    qWarning() << "EbookViewModel::setFocusedTokenIndex: Invalid index" << index
               << "- valid range is [-1," << static_cast<int>(m_tokenBoundaries.size()) - 1 << "]";
    return false;
}

const TokenInfo* EbookViewModel::getFocusedToken() const
{
    if (m_focusedTokenIndex >= 0 && m_focusedTokenIndex < static_cast<int>(m_tokenBoundaries.size())) {
        return &m_tokenBoundaries[m_focusedTokenIndex];
    }
    return nullptr;
}

bool EbookViewModel::moveFocusNext()
{
    if (m_tokenBoundaries.empty()) {
        return false;
    }

    if (m_focusedTokenIndex < 0) {
        // Start at the first token
        m_focusedTokenIndex = 0;
        return true;
    }

    if (m_focusedTokenIndex < static_cast<int>(m_tokenBoundaries.size()) - 1) {
        m_focusedTokenIndex++;
        return true;
    }

    // Already at the end
    return false;
}

bool EbookViewModel::moveFocusPrevious()
{
    if (m_tokenBoundaries.empty()) {
        return false;
    }

    if (m_focusedTokenIndex < 0) {
        // Start at the last token
        m_focusedTokenIndex = static_cast<int>(m_tokenBoundaries.size()) - 1;
        return true;
    }

    if (m_focusedTokenIndex > 0) {
        m_focusedTokenIndex--;
        return true;
    }

    // Already at the beginning
    return false;
}

std::vector<QPair<int,int>> EbookViewModel::addTermPositions(const Term& term)
{
    std::vector<QPair<int,int>> changed;

    if (m_text.isEmpty() || m_cachedTrieTokens.empty()) return changed;

    QString regexStr = m_langProvider->getWordRegex(m_language);
    bool charBased = m_langProvider->isCharacterBased(m_language);
    auto matchTok = Tokenizer::createRegex(regexStr, charBased);
    auto termResults = matchTok->tokenize(term.term);
    if (termResults.empty()) return changed;

    std::vector<std::string> termTokens;
    termTokens.reserve(termResults.size());
    for (const auto& r : termResults) {
        termTokens.push_back(toLowerTrieKey(r.text));
    }
    const int N = static_cast<int>(termTokens.size());

    std::vector<QPair<int,int>> spans;
    const int cacheSize = static_cast<int>(m_cachedTrieTokens.size());

    auto it = m_trieTokenIdxByFirstToken.find(termTokens[0]);
    if (it != m_trieTokenIdxByFirstToken.end()) {
        for (int i : it->second) {
            if (i + N > cacheSize) continue;
            bool match = true;
            for (int j = 1; j < N; ++j) {
                if (m_cachedTrieTokens[i + j] != termTokens[j]) { match = false; break; }
            }
            if (match) {
                int s = m_cachedMatchResults[i].startPos;
                int e = m_cachedMatchResults[i + N - 1].endPos;
                spans.push_back({s, e});
            }
        }
    }

    if (spans.empty()) {
        return changed;
    }

    // Merge new spans with existing positions, applying longest-span-wins.
    // Equal-length tiebreak favors the new entry so an update overwrites level.
    struct Cand { int s, e; Term t; bool isNew; };
    std::vector<Cand> all;
    all.reserve(m_termPositions.size() + spans.size());
    for (const auto& tp : m_termPositions) {
        all.push_back({tp.startPos, tp.endPos, tp.termData, false});
    }
    for (const auto& sp : spans) {
        all.push_back({sp.first, sp.second, term, true});
    }
    std::sort(all.begin(), all.end(), [](const Cand& a, const Cand& b) {
        if (a.s != b.s) return a.s < b.s;
        int aLen = a.e - a.s, bLen = b.e - b.s;
        if (aLen != bLen) return aLen > bLen;
        return a.isNew > b.isNew;
    });

    std::vector<TermPosition> next;
    next.reserve(all.size());
    int coveredUntil = INT_MIN;
    for (const auto& c : all) {
        if (c.s < coveredUntil) continue;
        TermPosition tp;
        tp.startPos = c.s;
        tp.endPos   = c.e;
        tp.termData = c.t;
        next.push_back(tp);
        coveredUntil = c.e;
    }

    int savedPos = (m_focusedTokenIndex >= 0 && m_focusedTokenIndex < static_cast<int>(m_tokenBoundaries.size()))
        ? m_tokenBoundaries[m_focusedTokenIndex].startPos
        : -1;

    m_termPositions = std::move(next);
    indexTermPositions();
    buildDisplayTokens();

    if (savedPos >= 0) {
        restoreFocusFromPosition(savedPos);
    }

    for (const auto& sp : spans) {
        changed.push_back(sp);
    }
    return changed;
}

std::vector<QPair<int,int>> EbookViewModel::removeTermPositions(const QString& termText)
{
    std::vector<QPair<int,int>> changed;
    if (m_termPositions.empty()) return changed;

    std::vector<TermPosition> next;
    next.reserve(m_termPositions.size());
    for (const auto& tp : m_termPositions) {
        if (tp.termData.term == termText) {
            changed.push_back({tp.startPos, tp.endPos});
        } else {
            next.push_back(tp);
        }
    }

    if (changed.empty()) return changed;

    int savedPos = (m_focusedTokenIndex >= 0 && m_focusedTokenIndex < static_cast<int>(m_tokenBoundaries.size()))
        ? m_tokenBoundaries[m_focusedTokenIndex].startPos
        : -1;

    m_termPositions = std::move(next);
    indexTermPositions();
    buildDisplayTokens();

    if (savedPos >= 0) {
        restoreFocusFromPosition(savedPos);
    }

    return changed;
}

TermPreview EbookViewModel::getPreviewForToken(const TokenInfo* token) const
{
    if (!token) {
        return TermPreview{};
    }

    const TermPosition* tp = findTermPosition(*token);
    if (tp) {
        return TermPreview{
            tp->termData.term,
            tp->termData.pronunciation,
            tp->termData.definition,
            true
        };
    }

    return TermPreview{token->text, "", "Click to add definition", false};
}

TermValidationResult EbookViewModel::validateTermLength(const QString& cleanedText) const
{
    LanguageConfig config = m_langProvider->getLanguageByCode(m_language);
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
    request.language = m_language;
    request.showWarning = false;
    request.exists = false;

    // Find the display token at this position
    int tokenIdx = findTokenAtPosition(position);
    if (tokenIdx < 0 || tokenIdx >= static_cast<int>(m_tokenBoundaries.size())) {
        request.termText = "";
        return request;
    }

    const TokenInfo& token = m_tokenBoundaries[tokenIdx];

    const TermPosition* tp = findTermPosition(token);
    if (tp && tp->termData.id > 0) {
        request.termText = tp->termData.term;
        request.exists = true;
        request.existingPronunciation = tp->termData.pronunciation;
        request.existingDefinition = tp->termData.definition;
        request.existingLevel = tp->termData.level;
    }

    // No exact match — use display token text as new-term suggestion
    if (!request.exists) {
        request.termText = token.text;
    }

    // Clean the text
    CleanedText cleaned = TermManager::cleanTermText(request.termText, m_language);
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
        request.exists = m_termStore->termExists(request.termText, m_language);
        if (request.exists) {
            Term existingTerm = m_termStore->getTerm(request.termText, m_language);
            request.existingPronunciation = existingTerm.pronunciation;
            request.existingDefinition = existingTerm.definition;
            request.existingLevel = existingTerm.level;
        }
    }

    return request;
}

EditRequest EbookViewModel::getEditRequestForSelection(int selectionStart, int selectionEnd) const
{
    EditRequest request;
    request.language = m_language;
    request.showWarning = false;
    request.exists = false;

    // Check if selection start is inside a known term (single or multi-word)
    int tokenIdx = findTokenAtPosition(selectionStart);
    if (tokenIdx >= 0) {
        const TermPosition* tp = findTermPosition(m_tokenBoundaries[tokenIdx]);
        if (tp) {
            request.termText = tp->termData.term;
            request.exists = true;
            request.existingPronunciation = tp->termData.pronunciation;
            request.existingDefinition = tp->termData.definition;
            request.existingLevel = tp->termData.level;
        }
    }

    // If no term at start, use token-snapped selection
    if (!request.exists) {
        QString selectedText = getTokenSelectionText(selectionStart, selectionEnd);
        if (!selectedText.isEmpty()) {
            request.termText = selectedText;
        }
    }

    // If still no text, return empty request
    if (request.termText.isEmpty()) {
        return request;
    }

    // Clean the text
    CleanedText cleaned = TermManager::cleanTermText(request.termText, m_language);
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
        request.exists = m_termStore->termExists(request.termText, m_language);
        if (request.exists) {
            Term existingTerm = m_termStore->getTerm(request.termText, m_language);
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
        emptyRequest.language = m_language;
        emptyRequest.showWarning = false;
        emptyRequest.exists = false;
        emptyRequest.termText = "";
        return emptyRequest;
    }

    return getEditRequestForPosition(token->startPos);
}

QPair<int, int> EbookViewModel::getFocusRange(int tokenIndex) const
{
    if (tokenIndex < 0 || tokenIndex >= static_cast<int>(m_tokenBoundaries.size())) {
        return qMakePair(-1, -1);
    }

    const TokenInfo& token = m_tokenBoundaries[tokenIndex];

    // A display token is known iff it exactly matches a termPosition,
    // so the focus range is always just the token's own span
    return qMakePair(token.startPos, token.endPos);
}

int EbookViewModel::findFirstTokenAtOrAfter(int pos) const
{
    auto it = std::lower_bound(m_tokenBoundaries.begin(), m_tokenBoundaries.end(), pos,
        [](const TokenInfo& t, int p) { return t.endPos <= p; });
    if (it != m_tokenBoundaries.end()) {
        return static_cast<int>(it - m_tokenBoundaries.begin());
    }
    return -1;
}

int EbookViewModel::findLastTokenAtOrBefore(int pos) const
{
    auto it = std::upper_bound(m_tokenBoundaries.begin(), m_tokenBoundaries.end(), pos,
        [](int p, const TokenInfo& t) { return p < t.startPos; });
    if (it == m_tokenBoundaries.begin()) return -1;
    return static_cast<int>(--it - m_tokenBoundaries.begin());
}

void EbookViewModel::restoreFocusFromPosition(int pos)
{
    int idx = findLastTokenAtOrBefore(pos);
    m_focusedTokenIndex = (idx < 0) ? 0 : idx;
}

void EbookViewModel::beginChunkedTermMatching()
{
    m_pendingTermPositions.clear();
    m_chunkScanIndex = 0;

    if (m_text.isEmpty()) {
        m_chunkTotalTokens = 0;
        return;
    }

    if (m_cachedMatchResults.empty()) {
        QString regexStr = m_langProvider->getWordRegex(m_language);
        bool charBased = m_langProvider->isCharacterBased(m_language);
        auto matchTok = Tokenizer::createRegex(regexStr, charBased);
        m_cachedMatchResults = matchTok->tokenize(m_text);

        m_cachedTrieTokens.clear();
        m_cachedTrieTokens.reserve(m_cachedMatchResults.size());
        for (const auto& r : m_cachedMatchResults) {
            m_cachedTrieTokens.push_back(toLowerTrieKey(r.text));
        }

        m_trieTokenIdxByFirstToken.clear();
        for (int i = 0; i < static_cast<int>(m_cachedTrieTokens.size()); ++i) {
            m_trieTokenIdxByFirstToken[m_cachedTrieTokens[i]].push_back(i);
        }
    }

    m_chunkTotalTokens = static_cast<int>(m_cachedTrieTokens.size());
}

bool EbookViewModel::processMatchChunk(int maxTokens)
{
    if (m_chunkScanIndex >= m_chunkTotalTokens) {
        return true;
    }

    TrieNode* trie = m_termStore->getTrieForLanguage(m_language);
    const auto& matchResults = m_cachedMatchResults;
    const auto& tokens = m_cachedTrieTokens;

    int processed = 0;
    int i = m_chunkScanIndex;

    while (i < m_chunkTotalTokens && processed < maxTokens) {
        Term* matchedTerm = trie->findLongestMatch(tokens, i);

        if (matchedTerm) {
            int startPos = matchResults[i].startPos;
            int endPos = matchResults[i + matchedTerm->tokenCount - 1].endPos;

            TermPosition termPos;
            termPos.startPos = startPos;
            termPos.endPos = endPos;
            termPos.termData = *matchedTerm;
            m_pendingTermPositions.push_back(termPos);

            i += matchedTerm->tokenCount;
            processed += matchedTerm->tokenCount;
        } else {
            i++;
            processed++;
        }
    }

    m_chunkScanIndex = i;
    return (m_chunkScanIndex >= m_chunkTotalTokens);
}

void EbookViewModel::commitTermMatches()
{
    m_termPositions = std::move(m_pendingTermPositions);
    m_pendingTermPositions.clear();

    indexTermPositions();
    buildDisplayTokens();
}
