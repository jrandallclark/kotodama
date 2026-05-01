#include "kotodama/tokenizer.h"
#include "kotodama/regextokenizer.h"
#include "kotodama/mecabtokenizer.h"

Tokenizer::Tokenizer(std::unique_ptr<TokenizerBackend> backend)
    : backend(std::move(backend))
{
}

std::vector<TokenResult> Tokenizer::tokenize(const QString& text) const
{
    if (!backend) return {};
    return backend->tokenize(text);
}

bool Tokenizer::isAvailable() const
{
    return backend && backend->isAvailable();
}

QString Tokenizer::backendName() const
{
    return backend ? backend->name() : QString("null");
}

std::unique_ptr<Tokenizer> Tokenizer::createRegex(
    const QString& pattern, bool splitIntoChars)
{
    return std::make_unique<Tokenizer>(
        std::make_unique<RegexTokenizer>(pattern, splitIntoChars));
}

std::unique_ptr<Tokenizer> Tokenizer::createJapanese()
{
    return std::make_unique<Tokenizer>(std::make_unique<MeCabTokenizer>());
}
