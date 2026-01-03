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
    auto mecab = std::make_unique<MeCabTokenizer>();
    if (mecab->isAvailable()) {
        return std::make_unique<Tokenizer>(std::move(mecab));
    }
    // Fallback: regex with character splitting for CJK script ranges
    QString jaPattern = "[\\x{3005}\\x{3040}-\\x{309F}\\x{30A0}-\\x{30FF}\\x{4E00}-\\x{9FFF}"
                        "\\x{3400}-\\x{4DBF}\\x{F900}-\\x{FAFF}\\x{FF66}-\\x{FF9F}]+";
    return createRegex(jaPattern, true);
}
