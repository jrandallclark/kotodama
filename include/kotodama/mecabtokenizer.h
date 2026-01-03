#ifndef MECABTOKENIZER_H
#define MECABTOKENIZER_H

#include "kotodama/tokenizerbackend.h"
#include <memory>

// MeCabTokenizer is always declared; isAvailable() returns false when
// HAVE_MECAB is not defined. The #ifdef is confined to mecabtokenizer.cpp,
// keeping mecab.h out of all consumer translation units.

class MeCabTokenizer : public TokenizerBackend
{
public:
    MeCabTokenizer();
    ~MeCabTokenizer();

    std::vector<TokenResult> tokenize(const QString& text) override;
    QString name() const override { return "mecab"; }
    bool isAvailable() const override;

private:
    class Impl;
    std::unique_ptr<Impl> d;  // null when HAVE_MECAB not defined
};

#endif // MECABTOKENIZER_H
