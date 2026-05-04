#ifndef HIGHLIGHTERSYNTAX_H
#define HIGHLIGHTERSYNTAX_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <array>
#include <vector>
#include <utility>
#include "term.h"

class EbookViewModel;

// Highlights text per-paragraph using the EbookViewModel token list.
// Each paragraph's highlightBlock linearly scans the tokens overlapping
// that paragraph and applies background colors per term level.
class TermHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    TermHighlighter(QTextDocument *document, EbookViewModel *model);

    void rehighlightForRanges(const std::vector<QPair<int,int>> &ranges);

protected:
    void highlightBlock(const QString &text) override;

private:
    void rebuildFormats();

    static constexpr int kNumLevels = 5;    // TermLevel::Recognized .. TermLevel::Ignored
    static constexpr int kUnknownIdx = 5;   // Format index for tokens without a known term
    static_assert(static_cast<int>(TermLevel::Ignored) + 1 == kNumLevels,
                  "kNumLevels out of sync with TermLevel enum");
    EbookViewModel *m_model;
    std::array<QTextCharFormat, kNumLevels + 1> m_formats;
};

#endif // HIGHLIGHTERSYNTAX_H
