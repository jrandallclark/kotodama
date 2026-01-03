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
            for (int i = 0; i < static_cast<int>(matched.length()); ++i) {
                tokens.push_back({
                    matched.mid(i, 1).toStdString(),
                    start + i,
                    start + i + 1
                });
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
