#ifndef REGEXTOKENIZER_H
#define REGEXTOKENIZER_H

#include "kotodama/tokenizerbackend.h"
#include <QRegularExpression>

class RegexTokenizer : public TokenizerBackend
{
public:
    RegexTokenizer(const QString& pattern, bool splitIntoChars = false);

    std::vector<TokenResult> tokenize(const QString& text) override;
    QString name() const override { return "regex"; }
    bool isRegex() const override { return true; }

private:
    QRegularExpression regex;
    bool splitIntoChars;
};

#endif // REGEXTOKENIZER_H
