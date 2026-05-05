#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "kotodama/tokenizerbackend.h"
#include <memory>
#include <vector>

class TokenizerBackend;

class Tokenizer
{
public:
    explicit Tokenizer(std::unique_ptr<TokenizerBackend> backend);

    std::vector<TokenResult> tokenize(const QString& text) const;
    bool isAvailable() const;
    QString backendName() const;
    bool isRegex() const;

    // Factory methods
    static std::unique_ptr<Tokenizer> createRegex(
        const QString& pattern, bool splitIntoChars = false);
    static std::unique_ptr<Tokenizer> createJapanese();

private:
    std::unique_ptr<TokenizerBackend> backend;
};

#endif // TOKENIZER_H
