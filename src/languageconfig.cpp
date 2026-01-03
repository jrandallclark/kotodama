#include "kotodama/languageconfig.h"
#include "kotodama/tokenizer.h"

LanguageConfig::~LanguageConfig() = default;
LanguageConfig::LanguageConfig(LanguageConfig&&) = default;
LanguageConfig& LanguageConfig::operator=(LanguageConfig&&) = default;

LanguageConfig::LanguageConfig()
    : isCharBasedValue(false)
    , tokenLimitValue(0)
{
}

LanguageConfig::LanguageConfig(const QString &name, const QString &code, const QString &wordRegex, bool isCharBased, int tokenLimit)
    : nameValue(name)
    , codeValue(code)
    , wordRegexValue(wordRegex)
    , isCharBasedValue(isCharBased)
    , tokenLimitValue(tokenLimit)
{
}

LanguageConfig::LanguageConfig(const LanguageConfig& other)
    : nameValue(other.nameValue)
    , codeValue(other.codeValue)
    , wordRegexValue(other.wordRegexValue)
    , isCharBasedValue(other.isCharBasedValue)
    , tokenLimitValue(other.tokenLimitValue)
    , tokenizerCache(nullptr)  // Reset cache — each copy creates its own lazily
{
}

LanguageConfig& LanguageConfig::operator=(const LanguageConfig& other)
{
    if (this != &other) {
        nameValue = other.nameValue;
        codeValue = other.codeValue;
        wordRegexValue = other.wordRegexValue;
        isCharBasedValue = other.isCharBasedValue;
        tokenLimitValue = other.tokenLimitValue;
        tokenizerCache.reset();  // Reset cache on assignment
    }
    return *this;
}

QString LanguageConfig::name() const
{
    return nameValue;
}

void LanguageConfig::setName(const QString &name)
{
    nameValue = name;
}

QString LanguageConfig::code() const
{
    return codeValue;
}

void LanguageConfig::setCode(const QString &code)
{
    codeValue = code;
}

QString LanguageConfig::wordRegex() const
{
    return wordRegexValue;
}

void LanguageConfig::setWordRegex(const QString &wordRegex)
{
    wordRegexValue = wordRegex;
    tokenizerCache.reset();  // Invalidate cache when regex changes
}

bool LanguageConfig::isCharBased() const
{
    return isCharBasedValue;
}

void LanguageConfig::setIsCharBased(bool isCharBased)
{
    isCharBasedValue = isCharBased;
    tokenizerCache.reset();  // Invalidate cache when char-based flag changes
}

int LanguageConfig::tokenLimit() const
{
    return tokenLimitValue;
}

void LanguageConfig::setTokenLimit(int tokenLimit)
{
    tokenLimitValue = tokenLimit;
}

const Tokenizer* LanguageConfig::tokenizer() const
{
    if (!tokenizerCache) {
        if (codeValue == "ja") {
            tokenizerCache = Tokenizer::createJapanese();
        } else {
            tokenizerCache = Tokenizer::createRegex(wordRegexValue, isCharBasedValue);
        }
    }
    return tokenizerCache.get();
}
