#include "kotodama/termmanager.h"
#include "kotodama/databasemanager.h"
#include "kotodama/languageconfig.h"
#include "kotodama/languagemanager.h"
#include "kotodama/tokenizer.h"
#include "kotodama/constants.h"
#include <QDebug>
#include <QRegularExpression>

TermManager& TermManager::instance()
{
    static TermManager instance;
    return instance;
}

TermManager::TermManager()
{
}

TermManager::~TermManager()
{
    clearCache();
}

void TermManager::clearLanguageData(const QString& language)
{
    // Delete trie
    if (tries.contains(language)) {
        delete tries[language];
        tries.remove(language);
    }

    // Delete terms
    if (terms.contains(language)) {
        for (Term* term : terms[language]) {
            delete term;
        }
        terms.remove(language);
    }
}

void TermManager::clearCache()
{
    // Delete all tries
    for (auto it = tries.begin(); it != tries.end(); ++it) {
        delete it.value();
    }
    tries.clear();

    // Delete all terms
    for (auto it = terms.begin(); it != terms.end(); ++it) {
        for (Term* term : it.value()) {
            delete term;
        }
    }
    terms.clear();
}

TrieNode* TermManager::getTrieForLanguage(const QString& language)
{
    // If trie already exists, return it
    if (tries.contains(language)) {
        return tries[language];
    }

    // Otherwise, build it
    buildTrieForLanguage(language);
    return tries[language];
}

void TermManager::buildTrieForLanguage(const QString& language)
{
    // Clear any existing data for this language
    clearLanguageData(language);

    // Create new trie
    TrieNode* trie = new TrieNode();
    tries[language] = trie;

    // Load terms from database
    QList<Term> dbTerms = DatabaseManager::instance().getTerms(language);

    // Trie tokenization must be context-free so the same term string always
    // produces the same tokens whether tokenized alone or inside a paragraph.
    // MeCab is context-sensitive — tokenizing "浮かん" alone vs. inside "浮かんだ"
    // can yield different morphemes, breaking trie lookup. Use char-based regex
    // for all languages (isCharacterBased is already true for ja/zh).
    QString regexStr = LanguageManager::instance().getWordRegex(language);
    bool charBased = LanguageManager::instance().isCharacterBased(language);
    auto tok = Tokenizer::createRegex(regexStr, charBased);

    // Store terms and build trie
    std::vector<Term*> languageTerms;

    for (const Term& t : std::as_const(dbTerms)) {
        Term* termPtr = new Term(t);
        languageTerms.push_back(termPtr);

        // Tokenize and insert into trie
        auto tokenResults = tok->tokenize(termPtr->term);
        std::vector<std::string> tokens;
        for (const auto& r : tokenResults) tokens.push_back(r.text);

        if (!tokens.empty()) {
            // Always update tokenCount from the current tokenizer — DB value may be stale
            // (e.g., term was added before MeCab was enabled, using char-based count)
            termPtr->tokenCount = static_cast<int>(tokens.size());
            trie->insert(termPtr, tokens);
        }
    }

    terms[language] = languageTerms;
}

bool TermManager::addTerm(const QString& term,
                          const QString& language,
                          TermLevel level,
                          const QString& definition,
                          const QString& pronunciation)
{
    // Add to database
    if (!DatabaseManager::instance().addTerm(term, language, level, definition, pronunciation)) {
        qDebug() << "[TermManager] Failed to add term:" << term << "for language:" << language
                 << "- Database error (possibly duplicate or database locked)";
        return false;
    }

    // Add just this term to the trie (much faster than full rebuild)
    addTermToTrie(term, language, level, definition, pronunciation);

    return true;
}

bool TermManager::updateTerm(const QString& term,
                             const QString& language,
                             TermLevel level,
                             const QString& definition,
                             const QString& pronunciation)
{
    // Update in database
    if (!DatabaseManager::instance().updateTerm(term, language, level, definition, pronunciation)) {
        qDebug() << "[TermManager] Failed to update term:" << term << "for language:" << language
                 << "- Database error (term may not exist or database locked)";
        return false;
    }

    // Update just this term in the trie (much faster than full rebuild)
    updateTermInTrie(term, language, level, definition, pronunciation);

    return true;
}

bool TermManager::updateTermLevel(const QString& term, const QString& language, TermLevel level)
{
    if (!DatabaseManager::instance().updateTermLevel(term, language, level)) {
        return false;
    }

    // Update just the level in the trie (very fast - no rebuild needed)
    if (terms.contains(language)) {
        for (Term* termPtr : terms[language]) {
            if (termPtr->term == term) {
                termPtr->level = level;
                // The trie points to this Term, so the update is automatic
                break;
            }
        }
    }

    return true;
}

bool TermManager::deleteTerm(const QString& term, const QString& language)
{
    if (!DatabaseManager::instance().deleteTerm(term, language)) {
        return false;
    }

    // Remove just this term from the trie (much faster than full rebuild)
    removeTermFromTrie(term, language);

    return true;
}

Term TermManager::getTerm(const QString& term, const QString& language)
{
    return DatabaseManager::instance().getTerm(term, language);
}

bool TermManager::termExists(const QString& term, const QString& language)
{
    Term t = DatabaseManager::instance().getTerm(term, language);
    return !t.term.isEmpty();
}

void TermManager::reloadLanguage(const QString& language)
{
    buildTrieForLanguage(language);
}

int TermManager::getTermCount(const QString& language)
{
    return DatabaseManager::instance().getTermCount(language);
}

CleanedText TermManager::cleanTermText(const QString& text, const QString& language)
{
    CleanedText result;

    QString original = text;

    // Get the regex for this language
    QString regexStr = LanguageManager::instance().getWordRegex(language);

    // Extract the character class from the regex pattern
    // Pattern is like: [\-'a-zA-Z...]+
    // We want just the part between [ and ]
    QString charClass = regexStr;
    if (charClass.startsWith("[") && charClass.contains("]")) {
        int start = charClass.indexOf("[") + 1;
        int end = charClass.indexOf("]");
        charClass = charClass.mid(start, end - start);
    }

    QString cleaned;
    QSet<QChar> removedChars;

    // Create pattern to test individual characters
    QRegularExpression charPattern(QString("[%1]").arg(charClass));

    for (QChar c : original) {
        if (c.isSpace()) {
            // Keep spaces for multi-word phrases
            cleaned.append(c);
        } else if (charPattern.match(QString(c)).hasMatch()) {
            // Valid character for this language
            cleaned.append(c);
        } else {
            // Invalid character
            removedChars.insert(c);
        }
    }

    // Clean up whitespace: remove leading/trailing and collapse multiple spaces
    cleaned = cleaned.simplified();

    result.cleaned = cleaned;

    // Build removed message
    if (!removedChars.isEmpty()) {
        QStringList removedList;
        for (QChar c : removedChars) {
            removedList.append(QString("\"%1\"").arg(c));
        }
        result.removed = QString("Removed: %1").arg(removedList.join(", "));
    }

    return result;
}

TermManager::ImportResult TermManager::batchAddTerms(const QList<TermToImport>& terms, bool updateDuplicates)
{
    ImportResult result;
    result.successCount = 0;
    result.failCount = 0;

    if (terms.isEmpty()) {
        return result;
    }

    // Group terms by language
    QMap<QString, QList<TermToImport>> termsByLanguage;
    for (const auto& term : terms) {
        termsByLanguage[term.language].append(term);
    }

    // Start a database transaction for better performance
    QSqlDatabase& db = DatabaseManager::instance().database();
    if (!db.transaction()) {
        result.errors.append("Failed to start database transaction");
        return result;
    }

    // Add or update all terms in database
    for (const auto& term : terms) {
        bool success = false;
        bool exists = termExists(term.term, term.language);

        if (exists && updateDuplicates) {
            // Update existing term
            success = DatabaseManager::instance().updateTerm(
                term.term,
                term.language,
                term.level,
                term.definition,
                term.pronunciation);
        } else if (exists && !updateDuplicates) {
            // Skip existing term
            continue;
        } else {
            // Add new term
            success = DatabaseManager::instance().addTerm(
                term.term,
                term.language,
                term.level,
                term.definition,
                term.pronunciation);
        }

        if (success) {
            result.successCount++;
        } else {
            result.failCount++;
            result.errors.append(QString("Failed to %1: %2")
                                .arg(exists ? "update" : "add")
                                .arg(term.term));
        }
    }

    // Commit the transaction
    if (!db.commit()) {
        db.rollback();
        result.errors.append("Database transaction failed - all changes rolled back");
        result.successCount = 0;
        result.failCount = terms.size();
        return result;
    }

    // Rebuild trie once for each language
    for (const QString& language : termsByLanguage.keys()) {
        reloadLanguage(language);
    }

    return result;
}

void TermManager::addTermToTrie(const QString& term, const QString& language, TermLevel level,
                                 const QString& definition, const QString& pronunciation)
{
    // Ensure trie exists for this language
    if (!tries.contains(language)) {
        buildTrieForLanguage(language);
        return;
    }

    // Create Term object
    Term* termPtr = new Term();
    termPtr->term = term;
    termPtr->language = language;
    termPtr->level = level;
    termPtr->definition = definition;
    termPtr->pronunciation = pronunciation;

    // Context-free trie tokenization — see buildTrieForLanguage comment.
    QString regexStr = LanguageManager::instance().getWordRegex(language);
    bool charBased = LanguageManager::instance().isCharacterBased(language);
    auto tok = Tokenizer::createRegex(regexStr, charBased);
    auto tokenResults = tok->tokenize(term);
    std::vector<std::string> tokens;
    for (const auto& r : tokenResults) tokens.push_back(r.text);

    if (!tokens.empty()) {
        termPtr->tokenCount = tokens.size();
        tries[language]->insert(termPtr, tokens);
        terms[language].push_back(termPtr);
    } else {
        delete termPtr;
    }
}

void TermManager::removeTermFromTrie(const QString& term, const QString& language)
{
    if (!terms.contains(language) || !tries.contains(language)) {
        return;
    }

    // Find the term in our vector
    Term* termPtr = nullptr;
    auto& languageTerms = terms[language];
    for (auto it = languageTerms.begin(); it != languageTerms.end(); ++it) {
        if ((*it)->term == term) {
            termPtr = *it;
            languageTerms.erase(it);
            break;
        }
    }

    if (!termPtr) {
        return; // Term not found
    }

    // Context-free trie tokenization — see buildTrieForLanguage comment.
    QString removeRegex = LanguageManager::instance().getWordRegex(language);
    bool removeCharBased = LanguageManager::instance().isCharacterBased(language);
    auto removeTok = Tokenizer::createRegex(removeRegex, removeCharBased);
    auto removeResults = removeTok->tokenize(term);
    std::vector<std::string> tokens;
    for (const auto& r : removeResults) tokens.push_back(r.text);

    // Remove from trie
    if (!tokens.empty()) {
        tries[language]->remove(tokens);
    }

    // Now safe to delete the term
    delete termPtr;
}

void TermManager::updateTermInTrie(const QString& term, const QString& language, TermLevel level,
                                    const QString& definition, const QString& pronunciation)
{
    if (!terms.contains(language)) {
        return;
    }

    // Find the term in our vector and update it in place
    for (Term* termPtr : terms[language]) {
        if (termPtr->term == term) {
            termPtr->level = level;
            termPtr->definition = definition;
            termPtr->pronunciation = pronunciation;
            // The trie already points to this Term object, so the update is reflected automatically
            return;
        }
    }
}


