#include "kotodama/highlightersyntax.h"
#include "kotodama/ebookviewmodel.h"
#include "kotodama/thememanager.h"
#include "kotodama/constants.h"

#include <QTextBlock>
#include <unordered_set>

TermHighlighter::TermHighlighter(QTextDocument *document, EbookViewModel *model)
    : QSyntaxHighlighter(document), m_model(model)
{
    rebuildFormats();

    // Re-derive format colors when the user toggles light/dark mode and
    // re-render every block — the document is unchanged but pixel output isn't.
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this]() {
                rebuildFormats();
                rehighlight();
            }, Qt::QueuedConnection);
}

void TermHighlighter::rebuildFormats()
{
    auto makeFormat = [](ThemeColor tc, bool brightCheck = false) {
        QTextCharFormat fmt;
        QColor bg = ThemeManager::instance().getColor(tc);
        if (bg != Qt::transparent) {
            fmt.setBackground(bg);
            if (brightCheck) {
                int brightness = (bg.red()   * Constants::Color::BRIGHTNESS_RED_WEIGHT +
                                  bg.green() * Constants::Color::BRIGHTNESS_GREEN_WEIGHT +
                                  bg.blue()  * Constants::Color::BRIGHTNESS_BLUE_WEIGHT)
                               / Constants::Color::BRIGHTNESS_DIVISOR;
                fmt.setForeground(brightness > Constants::Color::BRIGHTNESS_THRESHOLD
                                  ? Qt::black : Qt::white);
            }
        }
        return fmt;
    };

    m_formats[0] = makeFormat(ThemeColor::LevelRecognized);
    m_formats[1] = makeFormat(ThemeColor::LevelLearning);
    m_formats[2] = makeFormat(ThemeColor::LevelKnown);
    m_formats[3] = makeFormat(ThemeColor::LevelWellKnown, true);
    m_formats[4] = makeFormat(ThemeColor::LevelIgnored);
    m_formats[5] = makeFormat(ThemeColor::LevelUnknown);  // Unknown (no term)
}

void TermHighlighter::highlightBlock(const QString &text)
{
    QTextBlock block = currentBlock();
    int blockStart = block.position();
    int blockEnd   = blockStart + text.length();

    const auto &tokens = m_model->getTokenBoundaries();
    if (tokens.empty()) return;

    auto it = std::lower_bound(tokens.begin(), tokens.end(), blockStart,
        [](const TokenInfo &t, int pos) { return t.endPos <= pos; });

    while (it != tokens.end() && it->startPos < blockEnd) {
        int localStart = it->startPos - blockStart;
        int localEnd   = it->endPos   - blockStart;

        if (localStart < 0) localStart = 0;
        if (localEnd > text.length()) localEnd = text.length();
        int localLen = localEnd - localStart;

        if (localLen > 0) {
            int idx = it->hasTerm() ? static_cast<int>(*it->level) : kUnknownIdx;
            if (idx >= 0 && idx <= kUnknownIdx) {
                setFormat(localStart, localLen, m_formats[idx]);
            }
        }
        ++it;
    }
}

void TermHighlighter::rehighlightForRanges(const std::vector<QPair<int,int>> &ranges)
{
    QTextDocument *doc = document();
    if (!doc) return;

    // Walk every block touched by each range, not just first/last — a range
    // spanning 3+ paragraphs would otherwise leave middle blocks stale.
    std::unordered_set<int> done;
    for (const auto &range : ranges) {
        if (range.first >= range.second) continue;

        QTextBlock block = doc->findBlock(range.first);
        while (block.isValid() && block.position() < range.second) {
            if (done.insert(block.blockNumber()).second) {
                rehighlightBlock(block);
            }
            block = block.next();
        }
    }
}
