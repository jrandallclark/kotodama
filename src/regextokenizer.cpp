#include "kotodama/regextokenizer.h"

RegexTokenizer::RegexTokenizer(const QString& pattern, bool splitIntoChars)
    : regex(pattern)
    , splitIntoChars(splitIntoChars)
{
}

std::vector<TokenResult> RegexTokenizer::tokenize(const QString& text)
{
    std::vector<TokenResult> tokens;
    QRegularExpressionMatchIterator it = regex.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString matched = match.captured(0);
        if (matched.isEmpty()) continue;

        if (splitIntoChars) {
            int start = static_cast<int>(match.capturedStart());
            int i = 0;
            while (i < static_cast<int>(matched.length())) {
                int charLen = 1;
                if (matched.at(i).isHighSurrogate() && i + 1 < matched.length()
                    && matched.at(i + 1).isLowSurrogate()) {
                    charLen = 2;
                }
                tokens.push_back({
                    matched.mid(i, charLen).toStdString(),
                    start + i,
                    start + i + charLen
                });
                i += charLen;
            }
        } else {
            tokens.push_back({
                matched.toStdString(),
                static_cast<int>(match.capturedStart()),
                static_cast<int>(match.capturedEnd())
            });
        }
    }
    return tokens;
}
