#ifndef LANGUAGECONFIG_H
#define LANGUAGECONFIG_H

#include <QString>
#include <memory>

class Tokenizer;

class LanguageConfig
{
public:
    LanguageConfig();
    LanguageConfig(const QString &name, const QString &code, const QString &wordRegex, bool isCharBased, int tokenLimit);
    ~LanguageConfig();  // Defined in .cpp so unique_ptr<Tokenizer> destructs with complete type

    // Copy constructor resets tokenizer cache (each copy gets its own lazy cache)
    LanguageConfig(const LanguageConfig& other);
    LanguageConfig& operator=(const LanguageConfig& other);

    // Move — defined in .cpp so unique_ptr<Tokenizer> doesn't need Tokenizer complete in header
    LanguageConfig(LanguageConfig&&);
    LanguageConfig& operator=(LanguageConfig&&);

    QString name() const;
    void setName(const QString &name);

    QString code() const;
    void setCode(const QString &code);

    QString wordRegex() const;
    void setWordRegex(const QString &wordRegex);

    bool isCharBased() const;
    void setIsCharBased(bool isCharBased);

    int tokenLimit() const;
    void setTokenLimit(int tokenLimit);

    // Returns a non-owning pointer; valid for the lifetime of this LanguageConfig.
    const Tokenizer* tokenizer() const;

    // Returns true when the display tokenizer is not regex-based (e.g., MeCab).
    // When false, the trie-path regex already produces identical tokens and the
    // caller can reuse the trie-tokenization cache instead of re-tokenizing.
    bool needsDisplayTokenization() const;

private:
    QString nameValue;
    QString codeValue;
    QString wordRegexValue;
    bool isCharBasedValue;
    int tokenLimitValue;

    mutable std::unique_ptr<Tokenizer> tokenizerCache;
};

#endif // LANGUAGECONFIG_H
