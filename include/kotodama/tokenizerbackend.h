#ifndef TOKENIZERBACKEND_H
#define TOKENIZERBACKEND_H

#include <QString>
#include <vector>
#include <string>

struct TokenResult {
    std::string text;
    int startPos;  // character offset in source QString
    int endPos;    // exclusive end offset
};

class TokenizerBackend
{
public:
    virtual ~TokenizerBackend() = default;

    virtual std::vector<TokenResult> tokenize(const QString& text) = 0;
    virtual QString name() const = 0;
    virtual bool isAvailable() const { return true; }
};

#endif // TOKENIZERBACKEND_H
