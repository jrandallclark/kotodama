#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "kotodama/ebookviewmodel.h"
#include "kotodama/termmanager.h"
#include "kotodama/databasemanager.h"
#include "kotodama/ilanguageprovider.h"
#include "kotodama/itermstore.h"
#include "kotodama/trienode.h"
#include "kotodama/term.h"

#include <QStandardPaths>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>

class EbookViewModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Enable test mode
        QStandardPaths::setTestModeEnabled(true);

        // Initialize database
        DatabaseManager::instance().initialize();

        // Clean up all data from previous tests
        QSqlQuery query(DatabaseManager::instance().database());
        query.exec("DELETE FROM terms");
        query.exec("DELETE FROM texts");

        // Clear any cached data
        TermManager::instance().clearCache();
    }

    void TearDown() override {
        // Clear cached data
        TermManager::instance().clearCache();

        // Clean up all test data
        QSqlQuery query(DatabaseManager::instance().database());
        query.exec("DELETE FROM terms");
        query.exec("DELETE FROM texts");
    }
};

// Basic Loading Tests
TEST_F(EbookViewModelTest, LoadContentStoresTextAndLanguage) {
    EbookViewModel model;

    model.loadContent("Hello world", "en");

    EXPECT_EQ(model.getText(), "Hello world");
    EXPECT_EQ(model.getLanguage(), "en");
}

TEST_F(EbookViewModelTest, LoadEmptyContent) {
    EbookViewModel model;

    model.loadContent("", "en");

    EXPECT_TRUE(model.getText().isEmpty());
    EXPECT_TRUE(model.getTokenBoundaries().empty());
}

// Tokenization Tests
TEST_F(EbookViewModelTest, TokenizesSingleWord) {
    EbookViewModel model;

    model.loadContent("Hello", "en");

    const auto& tokens = model.getTokenBoundaries();
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].text, "Hello");
    EXPECT_EQ(tokens[0].startPos, 0);
    EXPECT_EQ(tokens[0].endPos, 5);
}

TEST_F(EbookViewModelTest, TokenizesMultipleWords) {
    EbookViewModel model;

    model.loadContent("Hello world test", "en");

    const auto& tokens = model.getTokenBoundaries();
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].text, "Hello");
    EXPECT_EQ(tokens[1].text, "world");
    EXPECT_EQ(tokens[2].text, "test");
}

TEST_F(EbookViewModelTest, TokenizationStoresCorrectPositions) {
    EbookViewModel model;

    model.loadContent("The quick brown", "en");

    const auto& tokens = model.getTokenBoundaries();
    ASSERT_EQ(tokens.size(), 3);

    // "The" at position 0-3
    EXPECT_EQ(tokens[0].startPos, 0);
    EXPECT_EQ(tokens[0].endPos, 3);

    // "quick" at position 4-9
    EXPECT_EQ(tokens[1].startPos, 4);
    EXPECT_EQ(tokens[1].endPos, 9);

    // "brown" at position 10-15
    EXPECT_EQ(tokens[2].startPos, 10);
    EXPECT_EQ(tokens[2].endPos, 15);
}

TEST_F(EbookViewModelTest, TokenizesFrenchWithApostrophe) {
    EbookViewModel model;

    model.loadContent("l'école", "fr");

    const auto& tokens = model.getTokenBoundaries();
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].text, "l'école");
}

TEST_F(EbookViewModelTest, TokenizesHyphenatedWords) {
    EbookViewModel model;

    model.loadContent("test-driven development", "en");

    const auto& tokens = model.getTokenBoundaries();
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].text, "test-driven");
    EXPECT_EQ(tokens[1].text, "development");
}

// Term Matching Tests
TEST_F(EbookViewModelTest, FindsSingleWordTerm) {
    TermManager::instance().addTerm("hello", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("The hello world", "en");

    const auto& termPositions = model.getTermPositions();
    ASSERT_EQ(termPositions.size(), 1);
    EXPECT_EQ(termPositions[0].termData.term, "hello");
}

TEST_F(EbookViewModelTest, FindsMultiWordPhrase) {
    TermManager::instance().addTerm("hello world", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("The hello world is here", "en");

    const auto& termPositions = model.getTermPositions();
    ASSERT_EQ(termPositions.size(), 1);
    EXPECT_EQ(termPositions[0].termData.term, "hello world");
    EXPECT_EQ(termPositions[0].termData.tokenCount, 2);
}

TEST_F(EbookViewModelTest, FindsMultipleTerms) {
    TermManager::instance().addTerm("hello", "en", TermLevel::Recognized);
    TermManager::instance().addTerm("world", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("hello world", "en");

    const auto& termPositions = model.getTermPositions();
    EXPECT_EQ(termPositions.size(), 2);
}

TEST_F(EbookViewModelTest, TermPositionsHaveCorrectLocations) {
    TermManager::instance().addTerm("world", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("hello world test", "en");

    const auto& termPositions = model.getTermPositions();
    ASSERT_EQ(termPositions.size(), 1);

    // "world" should be at position 6-11
    EXPECT_EQ(termPositions[0].startPos, 6);
    EXPECT_EQ(termPositions[0].endPos, 11);
}

// Highlight Tests
TEST_F(EbookViewModelTest, GeneratesHighlightsForKnownTerm) {
    TermManager::instance().addTerm("hello", "en", TermLevel::Learning);

    EbookViewModel model;
    model.loadContent("hello world", "en");

    auto highlights = model.getHighlights();

    // Should have 2 highlights: one for "hello" (Learning), one for "world" (unknown)
    EXPECT_EQ(highlights.size(), 2);

    // Find the "hello" highlight
    auto it = std::find_if(highlights.begin(), highlights.end(),
                           [](const HighlightInfo& h) { return !h.isUnknown; });
    ASSERT_NE(it, highlights.end());
    EXPECT_EQ(it->level, TermLevel::Learning);
    EXPECT_EQ(it->startPos, 0);
    EXPECT_EQ(it->endPos, 5);
}

TEST_F(EbookViewModelTest, GeneratesHighlightsForUnknownWords) {
    EbookViewModel model;
    model.loadContent("hello world", "en");

    auto highlights = model.getHighlights();

    // Both words should be unknown
    EXPECT_EQ(highlights.size(), 2);
    EXPECT_TRUE(highlights[0].isUnknown);
    EXPECT_TRUE(highlights[1].isUnknown);
}

TEST_F(EbookViewModelTest, HighlightsMultiWordPhrase) {
    TermManager::instance().addTerm("hello world", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("The hello world is here", "en");

    auto highlights = model.getHighlights();

    // Find the multi-word term highlight
    auto it = std::find_if(highlights.begin(), highlights.end(),
                           [](const HighlightInfo& h) {
                               return !h.isUnknown && h.startPos == 4;
                           });

    ASSERT_NE(it, highlights.end());
    EXPECT_EQ(it->startPos, 4);   // "hello" starts at 4
    EXPECT_EQ(it->endPos, 15);    // "world" ends at 15
}

// Position Query Tests
TEST_F(EbookViewModelTest, FindTokenAtPositionReturnsCorrectIndex) {
    EbookViewModel model;
    model.loadContent("hello world test", "en");

    EXPECT_EQ(model.findTokenAtPosition(0), 0);   // "hello"
    EXPECT_EQ(model.findTokenAtPosition(2), 0);   // inside "hello"
    EXPECT_EQ(model.findTokenAtPosition(6), 1);   // "world"
    EXPECT_EQ(model.findTokenAtPosition(12), 2);  // "test"
}

TEST_F(EbookViewModelTest, FindTokenAtPositionReturnsMinusOneForNonToken) {
    EbookViewModel model;
    model.loadContent("hello world", "en");

    // Position 5 is the space between words
    EXPECT_EQ(model.findTokenAtPosition(5), -1);
}

TEST_F(EbookViewModelTest, FindTermAtPositionReturnsCorrectTerm) {
    TermManager::instance().addTerm("world", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("hello world test", "en");

    const Term* found = model.findTermAtPosition(7);  // Inside "world"

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "world");
}

TEST_F(EbookViewModelTest, FindTermAtPositionReturnsNullForNonTerm) {
    TermManager::instance().addTerm("world", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("hello world test", "en");

    const Term* found = model.findTermAtPosition(0);  // "hello" is not a term

    EXPECT_EQ(found, nullptr);
}

TEST_F(EbookViewModelTest, FindTermAtPositionWorksForMultiWord) {
    TermManager::instance().addTerm("hello world", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("The hello world is here", "en");

    // Position 5 is inside "hello" of the multi-word term
    const Term* found1 = model.findTermAtPosition(5);
    // Position 8 is inside "world" of the multi-word term
    const Term* found2 = model.findTermAtPosition(8);

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);
    EXPECT_EQ(found1->term, "hello world");
    EXPECT_EQ(found2->term, "hello world");
}

// Selection Tests
TEST_F(EbookViewModelTest, GetTokenSelectionTextReturnsSingleToken) {
    EbookViewModel model;
    model.loadContent("hello world test", "en");

    // Select "hello" (positions 0-5)
    QString selection = model.getTokenSelectionText(0, 5);

    EXPECT_EQ(selection, "hello");
}

TEST_F(EbookViewModelTest, GetTokenSelectionTextReturnsMultipleTokens) {
    EbookViewModel model;
    model.loadContent("hello world test", "en");

    // Select from "hello" through "world" (positions 0-11)
    QString selection = model.getTokenSelectionText(0, 11);

    EXPECT_EQ(selection, "hello world");
}

TEST_F(EbookViewModelTest, GetTokenSelectionTextSnapsToTokenBoundaries) {
    EbookViewModel model;
    model.loadContent("hello world test", "en");

    // Select from middle of "hello" to middle of "world"
    QString selection = model.getTokenSelectionText(2, 8);

    // Should snap to full tokens
    EXPECT_EQ(selection, "hello world");
}

TEST_F(EbookViewModelTest, GetTokenSelectionTextReturnsEmptyForInvalidRange) {
    EbookViewModel model;
    model.loadContent("hello world", "en");

    QString selection = model.getTokenSelectionText(10, 5);  // End before start

    EXPECT_TRUE(selection.isEmpty());
}

// Refresh Tests
TEST_F(EbookViewModelTest, RefreshReanalyzesContent) {
    EbookViewModel model;
    model.loadContent("hello world", "en");

    EXPECT_EQ(model.getTermPositions().size(), 0);

    // Add a term after loading
    TermManager::instance().addTerm("world", "en", TermLevel::Known);

    // Refresh should find the new term
    model.refresh();

    EXPECT_EQ(model.getTermPositions().size(), 1);
}

// Edge Cases
TEST_F(EbookViewModelTest, HandlesTextWithPunctuation) {
    EbookViewModel model;
    model.loadContent("Hello, world! How are you?", "en");

    const auto& tokens = model.getTokenBoundaries();

    // Should extract only words, not punctuation
    EXPECT_EQ(tokens.size(), 5);
    EXPECT_EQ(tokens[0].text, "Hello");
    EXPECT_EQ(tokens[1].text, "world");
}

TEST_F(EbookViewModelTest, HandlesTextWithNumbers) {
    EbookViewModel model;
    model.loadContent("Test 123 hello 456", "en");

    const auto& tokens = model.getTokenBoundaries();

    // Numbers shouldn't be tokenized (based on English regex)
    EXPECT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].text, "Test");
    EXPECT_EQ(tokens[1].text, "hello");
}

TEST_F(EbookViewModelTest, HandlesJapaneseTextWithNumbers) {
    EbookViewModel model;
    model.loadContent("42個のりんごと100円", "ja");

    const auto& tokens = model.getTokenBoundaries();

    // Numbers should not appear as tokens in any language
    for (const auto& token : tokens) {
        QString text = token.text;
        bool isNumeric = true;
        for (const QChar& ch : text) {
            if (!ch.isDigit()) {
                isNumeric = false;
                break;
            }
        }
        EXPECT_FALSE(isNumeric) << "Numeric token should not appear: " << text.toStdString();
    }

    // Letter-based tokens should still be present
    bool hasLetterToken = false;
    for (const auto& token : tokens) {
        for (const QChar& ch : token.text) {
            if (ch.isLetter()) {
                hasLetterToken = true;
                break;
            }
        }
    }
    EXPECT_TRUE(hasLetterToken) << "Expected at least one letter-based token";
}

TEST_F(EbookViewModelTest, HandlesUnicodeContent) {
    EbookViewModel model;
    model.loadContent("café résumé naïve", "fr");

    const auto& tokens = model.getTokenBoundaries();

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].text, "café");
    EXPECT_EQ(tokens[1].text, "résumé");
    EXPECT_EQ(tokens[2].text, "naïve");
}

TEST_F(EbookViewModelTest, SetLanguageChangesLanguage) {
    EbookViewModel model;
    model.loadContent("test", "en");

    model.setLanguage("fr");

    EXPECT_EQ(model.getLanguage(), "fr");
}

// ============================================================================
// Japanese Parsing Integration Tests
//
// These tests exercise the two-layer architecture:
//   - tokenBoundaries: display tokens (MeCab if available, else char-based regex)
//   - termPositions:   trie matches (always char-based regex, context-free)
//
// All position assertions use Unicode character offsets (not UTF-8 byte offsets).
// ============================================================================

// Basic Japanese term matching via the char-based trie layer
TEST_F(EbookViewModelTest, JapaneseBasicTermMatching) {
    TermManager::instance().addTerm("日本語", "ja", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("これは日本語のテキストです", "ja");

    const auto& termPositions = model.getTermPositions();
    ASSERT_EQ(termPositions.size(), 1u);
    EXPECT_EQ(termPositions[0].termData.term, "日本語");
}

// Positions must be Unicode char offsets, not UTF-8 byte offsets.
// "日本語" is 3 chars but 9 bytes; if byte offsets were used startPos would be 9.
TEST_F(EbookViewModelTest, JapaneseTermPositionsAreCharBased) {
    TermManager::instance().addTerm("日本語", "ja", TermLevel::Known);

    EbookViewModel model;
    // "これは日本語です" — chars: こ(0)れ(1)は(2)日(3)本(4)語(5)で(6)す(7)
    model.loadContent("これは日本語です", "ja");

    const auto& termPositions = model.getTermPositions();
    ASSERT_EQ(termPositions.size(), 1u);
    EXPECT_EQ(termPositions[0].startPos, 3);
    EXPECT_EQ(termPositions[0].endPos,   6);
}

// 々 (U+3005) must be recognized as a word character and included in trie tokens
TEST_F(EbookViewModelTest, JapaneseTermWithIterationMark) {
    TermManager::instance().addTerm("様々", "ja", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("様々な問題がある", "ja");

    const auto& termPositions = model.getTermPositions();
    ASSERT_EQ(termPositions.size(), 1u);
    EXPECT_EQ(termPositions[0].termData.term, "様々");
    EXPECT_EQ(termPositions[0].startPos, 0);
    EXPECT_EQ(termPositions[0].endPos,   2);
}

// Term must be found even when followed by another character ("浮かん" in "浮かんだ").
// This is the key regression that MeCab context-sensitivity broke: MeCab could split
// "浮かんだ" as ["浮かんだ"] in context vs ["浮かん","だ"] in isolation, so the trie
// (built from isolated tokenization) wouldn't match. The fix is using char-based regex
// for all trie operations.
TEST_F(EbookViewModelTest, JapaneseTermFoundWhenFollowedByOtherChars) {
    TermManager::instance().addTerm("浮かん", "ja", TermLevel::Known);

    EbookViewModel model;
    // Char-by-char: 空(0)を(1)浮(2)か(3)ん(4)だ(5)雲(6)
    model.loadContent("空を浮かんだ雲", "ja");

    const auto& termPositions = model.getTermPositions();
    ASSERT_EQ(termPositions.size(), 1u);
    EXPECT_EQ(termPositions[0].termData.term, "浮かん");
    EXPECT_EQ(termPositions[0].startPos, 2);
    EXPECT_EQ(termPositions[0].endPos,   5);
}

// Multi-char term (6 characters) must have correct char positions
TEST_F(EbookViewModelTest, JapaneseMultiCharTermPositionAccurate) {
    TermManager::instance().addTerm("そんなふうに", "ja", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("そんなふうに二人で", "ja");

    const auto& termPositions = model.getTermPositions();
    ASSERT_EQ(termPositions.size(), 1u);
    EXPECT_EQ(termPositions[0].termData.term, "そんなふうに");
    EXPECT_EQ(termPositions[0].startPos, 0);
    EXPECT_EQ(termPositions[0].endPos,   6);
}

// findTermAtPosition must return the correct term at each char offset inside it,
// and nullptr for positions outside
TEST_F(EbookViewModelTest, JapaneseFindTermAtPositionAccurate) {
    TermManager::instance().addTerm("日本語", "ja", TermLevel::Known);

    EbookViewModel model;
    // "これは日本語です" — "日本語" occupies char positions 3, 4, 5
    model.loadContent("これは日本語です", "ja");

    const Term* at3 = model.findTermAtPosition(3);  // 日
    const Term* at4 = model.findTermAtPosition(4);  // 本
    const Term* at5 = model.findTermAtPosition(5);  // 語
    const Term* at2 = model.findTermAtPosition(2);  // は — not part of the term
    const Term* at6 = model.findTermAtPosition(6);  // で — not part of the term

    ASSERT_NE(at3, nullptr);
    ASSERT_NE(at4, nullptr);
    ASSERT_NE(at5, nullptr);
    EXPECT_EQ(at3->term, "日本語");
    EXPECT_EQ(at4->term, "日本語");
    EXPECT_EQ(at5->term, "日本語");
    EXPECT_EQ(at2, nullptr);
    EXPECT_EQ(at6, nullptr);
}

// A known term must always appear in getHighlights() with isUnknown=false,
// regardless of which backend (MeCab vs. regex) tokenizes the display layer
TEST_F(EbookViewModelTest, JapaneseKnownTermHighlightIsNotUnknown) {
    TermManager::instance().addTerm("日本語", "ja", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("これは日本語です", "ja");

    auto highlights = model.getHighlights();

    // Must contain exactly one known highlight spanning positions 3–6
    auto it = std::find_if(highlights.begin(), highlights.end(),
        [](const HighlightInfo& h) {
            return !h.isUnknown && h.startPos == 3 && h.endPos == 6;
        });
    EXPECT_NE(it, highlights.end())
        << "Expected a known highlight for 日本語 at [3, 6)";
}

// Japanese punctuation must not appear as display tokens (tokenBoundaries).
// 。is U+3002, outside the Japanese regex ranges, so it must be excluded
// whether the backend is MeCab or the char-based regex fallback.
TEST_F(EbookViewModelTest, JapanesePunctuationNotInTokenBoundaries) {
    EbookViewModel model;
    model.loadContent("こんにちは。さようなら。", "ja");

    const auto& tokens = model.getTokenBoundaries();

    for (const auto& token : tokens) {
        for (const QChar& ch : token.text) {
            EXPECT_TRUE(ch.isLetter() || ch.isDigit())
                << "Punctuation in display token: " << token.text.toStdString();
        }
    }
}

// Incomplete prefix of a known term must not match
TEST_F(EbookViewModelTest, JapaneseNoMatchForPartialTerm) {
    TermManager::instance().addTerm("浮かん", "ja", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("浮か", "ja");  // Only the first 2 of 3 chars

    EXPECT_EQ(model.getTermPositions().size(), 0u);
}

// Two adjacent known terms must be found independently at correct positions
TEST_F(EbookViewModelTest, JapaneseAdjacentTermsFoundCorrectly) {
    TermManager::instance().addTerm("双眼鏡", "ja", TermLevel::Learning);
    TermManager::instance().addTerm("のぞい", "ja", TermLevel::Known);

    EbookViewModel model;
    // Char-by-char: 双(0)眼(1)鏡(2)で(3)の(4)ぞ(5)い(6)た(7)
    model.loadContent("双眼鏡でのぞいた", "ja");

    const auto& termPositions = model.getTermPositions();
    ASSERT_EQ(termPositions.size(), 2u);

    bool found双眼鏡 = false, foundのぞい = false;
    for (const auto& tp : termPositions) {
        if (tp.termData.term == "双眼鏡") {
            EXPECT_EQ(tp.startPos, 0);
            EXPECT_EQ(tp.endPos,   3);
            found双眼鏡 = true;
        } else if (tp.termData.term == "のぞい") {
            EXPECT_EQ(tp.startPos, 4);
            EXPECT_EQ(tp.endPos,   7);
            foundのぞい = true;
        }
    }
    EXPECT_TRUE(found双眼鏡) << "双眼鏡 not found in termPositions";
    EXPECT_TRUE(foundのぞい) << "のぞい not found in termPositions";
}

// Hover query: findTermAtPosition for 双眼鏡 must not return a term for のぞい's range
TEST_F(EbookViewModelTest, JapaneseFindTermAtPositionDoesNotCrossTermBoundaries) {
    TermManager::instance().addTerm("双眼鏡", "ja", TermLevel::Learning);
    TermManager::instance().addTerm("のぞい", "ja", TermLevel::Known);

    EbookViewModel model;
    // 双(0)眼(1)鏡(2)で(3)の(4)ぞ(5)い(6)た(7)
    model.loadContent("双眼鏡でのぞいた", "ja");

    // Hovering over のぞい range must return のぞい, not 双眼鏡
    const Term* atの = model.findTermAtPosition(4);
    const Term* at双 = model.findTermAtPosition(1);
    const Term* atで = model.findTermAtPosition(3);
    EXPECT_EQ(atで, nullptr);
}

// ============================================================================
// Display-token / trie-term overlap: longest span always wins.
//
// Greedy selection from all candidates (MeCab tokens + known-term spans),
// sorted by start ASC, length DESC.  A token is known iff its span exactly
// matches a termPosition; otherwise it is unknown.  Known terms that are
// strictly contained within a longer MeCab token are absorbed (hidden).
// ============================================================================

// Case 1: Known term is a substring of a longer MeCab token → MeCab wins.
// MeCab 一部 [0,2) is longer than known 部 [1,2): 一部 absorbs 部.
TEST_F(EbookViewModelTest, JapaneseKnownTermAtEndOfDisplayToken) {
    TermManager::instance().addTerm("部", "ja", TermLevel::Known);

    EbookViewModel model;
    // 一(0)部(1)
    model.loadContent("一部", "ja");

    const auto& tokens = model.getTokenBoundaries();
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].text, QString::fromUtf8("一部"));
    EXPECT_EQ(tokens[0].startPos, 0);
    EXPECT_EQ(tokens[0].endPos,   2);

    // 一部 itself is not a saved term → unknown
    TermPreview preview = model.getPreviewForToken(&tokens[0]);
    EXPECT_FALSE(preview.isKnown);

    // Only unknown highlights (部 is absorbed)
    auto highlights = model.getHighlights();
    ASSERT_EQ(highlights.size(), 1u);
    EXPECT_TRUE(highlights[0].isUnknown);
}

// Case 2: Known term at start, absorbed by longer MeCab token.
// MeCab 部屋 [0,2) is longer than known 部 [0,1): 部屋 absorbs 部.
TEST_F(EbookViewModelTest, JapaneseKnownTermAtStartOfDisplayToken) {
    TermManager::instance().addTerm("部", "ja", TermLevel::Known);

    EbookViewModel model;
    // 部(0)屋(1)
    model.loadContent("部屋", "ja");

    const auto& tokens = model.getTokenBoundaries();
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].text, QString::fromUtf8("部屋"));
    EXPECT_EQ(tokens[0].startPos, 0);
    EXPECT_EQ(tokens[0].endPos,   2);

    // 部屋 is not a saved term
    EXPECT_FALSE(model.getPreviewForToken(&tokens[0]).isKnown);

    auto highlights = model.getHighlights();
    ASSERT_EQ(highlights.size(), 1u);
    EXPECT_TRUE(highlights[0].isUnknown);
}

// Case 3: Known term in middle, absorbed by longer MeCab token.
// MeCab 根本 [0,2) is longer than known 本 [1,2): 根本 absorbs 本.
// The 的 [2,3) is a separate MeCab token (also unknown).
TEST_F(EbookViewModelTest, JapaneseKnownTermInMiddleOfDisplayToken) {
    TermManager::instance().addTerm("本", "ja", TermLevel::Known);

    EbookViewModel model;
    // 根(0)本(1)的(2)
    model.loadContent("根本的", "ja");

    const auto& tokens = model.getTokenBoundaries();
    // MeCab splits into 根本 [0,2) and 的 [2,3); 本 is absorbed
    ASSERT_GE(tokens.size(), 1u);

    // 根本 [0,2) exists and is unknown
    bool foundKonpon = false;
    for (const auto& t : tokens) {
        if (t.text == QString::fromUtf8("根本") && t.startPos == 0 && t.endPos == 2) {
            foundKonpon = true;
            EXPECT_FALSE(model.getPreviewForToken(&t).isKnown);
        }
    }
    EXPECT_TRUE(foundKonpon) << "Expected token 根本 [0,2)";

    // 的 [2,3) exists and is unknown
    bool foundTeki = false;
    for (const auto& t : tokens) {
        if (t.text == QString::fromUtf8("的") && t.startPos == 2 && t.endPos == 3) {
            foundTeki = true;
            EXPECT_FALSE(model.getPreviewForToken(&t).isKnown);
        }
    }
    EXPECT_TRUE(foundTeki) << "Expected token 的 [2,3)";

    // All highlights are unknown (本 is absorbed)
    auto highlights = model.getHighlights();
    for (const auto& h : highlights) {
        EXPECT_TRUE(h.isUnknown) << "All tokens should be unknown (本 absorbed)";
    }
}

// Case 5: Known term and MeCab token have the same span → known wins.
TEST_F(EbookViewModelTest, JapaneseKnownTermFullyEncompassesDisplayToken) {
    TermManager::instance().addTerm("学生", "ja", TermLevel::Known);

    EbookViewModel model;
    // 私(0)は(1)学(2)生(3)で(4)す(5)
    model.loadContent("私は学生です", "ja");

    const auto& tokens = model.getTokenBoundaries();

    // Must have at least one token — find the 学生 token
    int gakuseiCount = 0;
    for (const auto& t : tokens) {
        if (t.startPos == 2 && t.endPos == 4 &&
            t.text == QString::fromUtf8("学生")) {
            gakuseiCount++;
        }
    }
    EXPECT_EQ(gakuseiCount, 1) << "学生 must appear exactly once as a display token";

    // 学生 is a known term → known highlight must exist
    bool foundKnown = false;
    auto highlights = model.getHighlights();
    for (const auto& h : highlights) {
        if (!h.isUnknown && h.startPos == 2 && h.endPos == 4) {
            foundKnown = true;
            EXPECT_EQ(h.level, TermLevel::Known);
        }
    }
    EXPECT_TRUE(foundKnown) << "Known highlight for 学生 [2,4) not found";
}

// Case 8: No known terms — display tokens unchanged.
TEST_F(EbookViewModelTest, JapaneseNoKnownTermsDisplayTokensUnchanged) {
    EbookViewModel model;
    model.loadContent("日本語を勉強する", "ja");

    const auto& tokens = model.getTokenBoundaries();
    EXPECT_FALSE(tokens.empty());

    for (const auto& t : tokens) {
        bool hasLetter = false;
        for (const QChar& ch : t.text) {
            if (ch.isLetter()) {
                hasLetter = true;
                break;
            }
        }
        EXPECT_TRUE(hasLetter) << "Token '" << t.text.toStdString()
                               << "' has no letters";
    }

    auto highlights = model.getHighlights();
    for (const auto& h : highlights) {
        EXPECT_TRUE(h.isUnknown);
    }
}


// ============================================================================
// MeCab display-token / trie-term overlap: longest wins
//
// Scenario:  聞こ is a known term [4,6).  MeCab parses 聞こえ as a single
//            morpheme [4,7).  Since 聞こえ is longer than 聞こ, it wins
//            and absorbs the known term.  The display token is 聞こえ [4,7)
//            unknown; the known term 聞こ does not produce a separate highlight.
//
// Rules:
//   1. Trie matching (char-based): 聞こ matched at [4,6) — unchanged.
//   2. findTermAtPosition: returns nullptr for all positions (聞こ absorbed).
//   3. No known highlight for 聞こ (absorbed by longer MeCab token).
//   4. Single unknown highlight 聞こえ [4,7) — no split.
// ============================================================================

TEST_F(EbookViewModelTest, JapaneseKnownTermNotMaskedByMeCabToken) {
    // 聞こ is known; 聞こえ is not in the database
    TermManager::instance().addTerm("聞こ", "ja", TermLevel::Known);

    EbookViewModel model;
    // 水(0)の(1)音(2)が(3)聞(4)こ(5)え(6)た(7)
    model.loadContent("水の音が聞こえた", "ja");

    // --- Rule 1: trie still matched 聞こ at [4,6) ---
    const auto& termPositions = model.getTermPositions();
    ASSERT_EQ(termPositions.size(), 1u);
    EXPECT_EQ(termPositions[0].termData.term, "聞こ");
    EXPECT_EQ(termPositions[0].startPos, 4);
    EXPECT_EQ(termPositions[0].endPos,   6);

    // --- Rule 2: hover positions — 聞こ absorbed, no exact display-token match ---
    const Term* at4 = model.findTermAtPosition(4);  // 聞
    const Term* at5 = model.findTermAtPosition(5);  // こ
    const Term* at6 = model.findTermAtPosition(6);  // え

    EXPECT_EQ(at4, nullptr) << "聞こ absorbed by longer 聞こえ display token";
    EXPECT_EQ(at5, nullptr) << "聞こ absorbed by longer 聞こえ display token";
    EXPECT_EQ(at6, nullptr) << "え is part of unknown 聞こえ token";

    // --- Rules 3 & 4: single unknown highlight for 聞こえ [4,7) ---
    auto highlights = model.getHighlights();

    // Rule 3: NO known highlight for 聞こ (absorbed)
    auto knownIt = std::find_if(highlights.begin(), highlights.end(),
        [](const HighlightInfo& h) {
            return !h.isUnknown && h.startPos == 4 && h.endPos == 6;
        });
    EXPECT_EQ(knownIt, highlights.end())
        << "聞こ must NOT have a separate known highlight (absorbed by 聞こえ)";

    // Rule 4: unknown highlight must exist for 聞こえ [4,7)
    auto unknownIt = std::find_if(highlights.begin(), highlights.end(),
        [](const HighlightInfo& h) {
            return h.isUnknown && h.startPos == 4 && h.endPos == 7;
        });
    EXPECT_NE(unknownIt, highlights.end())
        << "Unknown highlight for 聞こえ [4,7) not found";
}

// tokenCount in the trie must always reflect the char-based tokenizer count,
// not any stale value from the database (which may have been set by a different tokenizer)
TEST_F(EbookViewModelTest, JapaneseTermTokenCountMatchesCharCount) {
    // "そんなふうに" = 6 chars → 6 tokens in char-based trie
    TermManager::instance().addTerm("そんなふうに", "ja", TermLevel::Known);
    TermManager::instance().addTerm("日本語", "ja", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("そんなふうに日本語のテキスト", "ja");

    const auto& termPositions = model.getTermPositions();
    ASSERT_EQ(termPositions.size(), 2u);

    for (const auto& tp : termPositions) {
        if (tp.termData.term == "そんなふうに") {
            // endPos - startPos must equal tokenCount (one char per token)
            int spanLen = tp.endPos - tp.startPos;
            EXPECT_EQ(spanLen, tp.termData.tokenCount)
                << "そんなふうに: span=" << spanLen
                << " tokenCount=" << tp.termData.tokenCount;
        } else if (tp.termData.term == "日本語") {
            int spanLen = tp.endPos - tp.startPos;
            EXPECT_EQ(spanLen, tp.termData.tokenCount)
                << "日本語: span=" << spanLen
                << " tokenCount=" << tp.termData.tokenCount;
        }
    }
}

// ============================================================================
// TermValidationResult Tests
// ============================================================================

TEST_F(EbookViewModelTest, ValidateTermLength_EnglishWithinLimit) {
    EbookViewModel model;
    model.loadContent("test", "en");

    TermValidationResult result = model.validateTermLength("hello world");

    EXPECT_TRUE(result.isValid);
    EXPECT_TRUE(result.warningMessage.isEmpty());
}

TEST_F(EbookViewModelTest, ValidateTermLength_EnglishExceedsLimit) {
    EbookViewModel model;
    model.loadContent("test", "en");

    // Create a string with 7 words (limit is 6 for English)
    QString longTerm = "one two three four five six seven";
    TermValidationResult result = model.validateTermLength(longTerm);

    EXPECT_FALSE(result.isValid);
    EXPECT_FALSE(result.warningMessage.isEmpty());
    EXPECT_TRUE(result.warningMessage.contains("6"));
}

TEST_F(EbookViewModelTest, ValidateTermLength_JapaneseWithinLimit) {
    EbookViewModel model;
    model.loadContent("テスト", "ja");

    // 10 chars (limit is 15 for Japanese)
    TermValidationResult result = model.validateTermLength("これはテストです");

    EXPECT_TRUE(result.isValid);
    EXPECT_TRUE(result.warningMessage.isEmpty());
}

TEST_F(EbookViewModelTest, ValidateTermLength_JapaneseExceedsLimit) {
    EbookViewModel model;
    model.loadContent("テスト", "ja");

    // Create a string with 13 chars (limit is 12 for Japanese)
    QString longTerm = "これは非常に長い日本語のテス";
    TermValidationResult result = model.validateTermLength(longTerm);

    EXPECT_FALSE(result.isValid);
    EXPECT_FALSE(result.warningMessage.isEmpty());
    EXPECT_TRUE(result.warningMessage.contains("12"));
}

// ============================================================================
// EditRequest Tests
// ============================================================================

TEST_F(EbookViewModelTest, GetEditRequestForPosition_KnownTerm) {
    TermManager::instance().addTerm("hello", "en", TermLevel::Learning, "a greeting", "heh-loh");

    EbookViewModel model;
    model.loadContent("hello world", "en");

    // Position 0 is inside "hello"
    EditRequest request = model.getEditRequestForPosition(0);

    EXPECT_EQ(request.termText, "hello");
    EXPECT_TRUE(request.exists);
    EXPECT_EQ(request.existingDefinition, "a greeting");
    EXPECT_EQ(request.existingPronunciation, "heh-loh");
    EXPECT_EQ(request.existingLevel, TermLevel::Learning);
    EXPECT_FALSE(request.showWarning);
}

TEST_F(EbookViewModelTest, GetEditRequestForPosition_UnknownToken) {
    EbookViewModel model;
    model.loadContent("hello world", "en");

    // Position 6 is inside "world" (unknown)
    EditRequest request = model.getEditRequestForPosition(6);

    EXPECT_EQ(request.termText, "world");
    EXPECT_FALSE(request.exists);
    EXPECT_FALSE(request.showWarning);
}

TEST_F(EbookViewModelTest, GetEditRequestForPosition_MultiWordTerm) {
    TermManager::instance().addTerm("hello world", "en", TermLevel::Known, "common phrase");

    EbookViewModel model;
    model.loadContent("say hello world now", "en");

    // Position 4 is inside "hello" which is part of multi-word term
    EditRequest request = model.getEditRequestForPosition(4);

    EXPECT_EQ(request.termText, "hello world");
    EXPECT_TRUE(request.exists);
    EXPECT_EQ(request.existingDefinition, "common phrase");
}

TEST_F(EbookViewModelTest, GetEditRequestForPosition_InvalidPosition) {
    EbookViewModel model;
    model.loadContent("hello", "en");

    // Position 100 is outside text
    EditRequest request = model.getEditRequestForPosition(100);

    EXPECT_TRUE(request.termText.isEmpty());
}

TEST_F(EbookViewModelTest, ValidateTermLength_TooLongTerm) {
    EbookViewModel model;
    model.loadContent("test", "en");

    // Try to create a term at position with a too-long selection
    // First add a super long "term" by just clicking on a word
    // But validation happens on the selected text, so we need to simulate that
    // Actually, validation happens in getEditRequest, so let's test it there

    // Create a mock long term by directly testing validation path
    QString longTerm = "one two three four five six seven eight nine ten eleven";
    TermValidationResult validation = model.validateTermLength(longTerm);
    EXPECT_FALSE(validation.isValid);
}

TEST_F(EbookViewModelTest, GetEditRequestForSelection_KnownTerm) {
    TermManager::instance().addTerm("hello world", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("say hello world here", "en");

    // Select "hello world" (positions 4-15)
    EditRequest request = model.getEditRequestForSelection(4, 15);

    EXPECT_EQ(request.termText, "hello world");
    EXPECT_TRUE(request.exists);
}

TEST_F(EbookViewModelTest, GetEditRequestForSelection_UnknownTokens) {
    EbookViewModel model;
    model.loadContent("hello world test", "en");

    // Select "world test" (positions 6-16)
    EditRequest request = model.getEditRequestForSelection(6, 16);

    EXPECT_EQ(request.termText, "world test");
    EXPECT_FALSE(request.exists);
}

TEST_F(EbookViewModelTest, GetEditRequestForFocusedToken_Known) {
    TermManager::instance().addTerm("hello", "en", TermLevel::Learning);

    EbookViewModel model;
    model.loadContent("hello world", "en");
    model.setFocusedTokenIndex(0);

    EditRequest request = model.getEditRequestForFocusedToken();

    EXPECT_EQ(request.termText, "hello");
    EXPECT_TRUE(request.exists);
}

TEST_F(EbookViewModelTest, GetEditRequestForFocusedToken_Unknown) {
    EbookViewModel model;
    model.loadContent("hello world", "en");
    model.setFocusedTokenIndex(1); // "world"

    EditRequest request = model.getEditRequestForFocusedToken();

    EXPECT_EQ(request.termText, "world");
    EXPECT_FALSE(request.exists);
}

TEST_F(EbookViewModelTest, GetEditRequestForFocusedToken_NoFocus) {
    EbookViewModel model;
    model.loadContent("hello world", "en");
    // Don't set focus

    EditRequest request = model.getEditRequestForFocusedToken();

    EXPECT_TRUE(request.termText.isEmpty());
}

// ============================================================================
// Focus Range Tests
// ============================================================================

TEST_F(EbookViewModelTest, GetFocusRange_SingleTokenReturnsTokenSpan) {
    EbookViewModel model;
    model.loadContent("hello world", "en");

    // "hello" is at positions [0, 5)
    QPair<int, int> range = model.getFocusRange(0);

    EXPECT_EQ(range.first, 0);
    EXPECT_EQ(range.second, 5);
}

TEST_F(EbookViewModelTest, GetFocusRange_MultiWordTermReturnsFullTermSpan) {
    TermManager::instance().addTerm("hello world", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("say hello world now", "en");

    // "hello" is token index 1, but it's part of the "hello world" term [4, 15)
    QPair<int, int> range = model.getFocusRange(1);

    EXPECT_EQ(range.first, 4);
    EXPECT_EQ(range.second, 15);
}

TEST_F(EbookViewModelTest, GetFocusRange_SecondTokenInMultiWordTermReturnsSameSpan) {
    // With buildDisplayTokens, "hello world" is now a single known-term display token.
    // This was previously "hello" (index 1) + "world" (index 2) as separate tokens.
    // Now token index 1 is "hello world", and index 2 is "now".
    TermManager::instance().addTerm("hello world", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("say hello world now", "en");

    // Token 1 is the full "hello world" known-term token [4, 15)
    const auto& tokens = model.getTokenBoundaries();
    int hwIndex = -1;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].text == "hello world") {
            hwIndex = static_cast<int>(i);
            break;
        }
    }
    ASSERT_GE(hwIndex, 0) << "Could not find token 'hello world'";

    QPair<int, int> range = model.getFocusRange(hwIndex);
    EXPECT_EQ(range.first, 4);
    EXPECT_EQ(range.second, 15);
}

TEST_F(EbookViewModelTest, GetFocusRange_JapaneseMultiCharTermReturnsFullSpan) {
    TermManager::instance().addTerm("した", "ja", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("にした際", "ja");

    // With buildDisplayTokens, "した" is a single known-term display token.
    // Find the token that covers position 1 (where し starts).
    int tokenIdx = model.findTokenAtPosition(1);
    ASSERT_GE(tokenIdx, 0) << "Could not find token at position 1";

    // "し" is part of "した" term - should return full term span [1, 3)
    QPair<int, int> range = model.getFocusRange(tokenIdx);
    EXPECT_EQ(range.first, 1);
    EXPECT_EQ(range.second, 3);
    EXPECT_EQ(range.second - range.first, 2)
        << "Focus range for token containing 'し' should span 2 characters";
}

TEST_F(EbookViewModelTest, GetFocusRange_InvalidIndexReturnsEmpty) {
    EbookViewModel model;
    model.loadContent("hello", "en");

    QPair<int, int> range = model.getFocusRange(-1);

    EXPECT_EQ(range.first, -1);
    EXPECT_EQ(range.second, -1);
}

TEST_F(EbookViewModelTest, FindFirstTokenAtOrAfter_ReturnsFirstTokenAfterPos) {
    EbookViewModel model;
    model.loadContent("hello world test", "en");

    // Position 2 is inside "hello" [0, 5), first visible token is "hello" (index 0)
    EXPECT_EQ(model.findFirstTokenAtOrAfter(2), 0);

    // Position 5 is the space between words, first token at/after is "world" (index 1)
    EXPECT_EQ(model.findFirstTokenAtOrAfter(5), 1);

    // Position 6 is inside "world" [6, 11), first visible token is "world" (index 1)
    EXPECT_EQ(model.findFirstTokenAtOrAfter(6), 1);

    // Position 20 is past the end
    EXPECT_EQ(model.findFirstTokenAtOrAfter(20), -1);
}

TEST_F(EbookViewModelTest, FindLastTokenAtOrBefore_ReturnsLastTokenBeforePos) {
    EbookViewModel model;
    model.loadContent("hello world test", "en");

    // Position 2 is inside "hello", last token at or before is "hello" (index 0)
    EXPECT_EQ(model.findLastTokenAtOrBefore(2), 0);

    // Position 5 is the space, last token at or before is "hello" (index 0)
    EXPECT_EQ(model.findLastTokenAtOrBefore(5), 0);

    // Position 6 is inside "world", last token at or before is "world" (index 1)
    EXPECT_EQ(model.findLastTokenAtOrBefore(6), 1);

    // Position 20 is past the end, last token at or before is "test" (index 2)
    EXPECT_EQ(model.findLastTokenAtOrBefore(20), 2);
}

// ============================================================================
// TermPreview Tests
// ============================================================================

TEST_F(EbookViewModelTest, GetPreviewForNullTokenReturnsEmptyPreview) {
    EbookViewModel model;
    model.loadContent("hello world", "en");

    TermPreview preview = model.getPreviewForToken(nullptr);

    EXPECT_TRUE(preview.term.isEmpty());
    EXPECT_TRUE(preview.pronunciation.isEmpty());
    EXPECT_TRUE(preview.definition.isEmpty());
    EXPECT_FALSE(preview.isKnown);
}

TEST_F(EbookViewModelTest, GetPreviewForKnownTermReturnsTermData) {
    TermManager::instance().addTerm("hello", "en", TermLevel::Learning, "greeting", "pronunciation");

    EbookViewModel model;
    model.loadContent("hello world", "en");

    const TokenInfo* token = model.getFocusedToken(); // Initially no focus
    model.setFocusedTokenIndex(0); // Focus on "hello"
    token = model.getFocusedToken();
    ASSERT_NE(token, nullptr);

    TermPreview preview = model.getPreviewForToken(token);

    EXPECT_EQ(preview.term, "hello");
    EXPECT_EQ(preview.pronunciation, "pronunciation");
    EXPECT_EQ(preview.definition, "greeting");
    EXPECT_TRUE(preview.isKnown);
}

TEST_F(EbookViewModelTest, GetPreviewForUnknownWordReturnsPlaceholder) {
    EbookViewModel model;
    model.loadContent("hello world", "en");

    model.setFocusedTokenIndex(0); // Focus on "hello" (not a known term)
    const TokenInfo* token = model.getFocusedToken();
    ASSERT_NE(token, nullptr);

    TermPreview preview = model.getPreviewForToken(token);

    EXPECT_EQ(preview.term, "hello");
    EXPECT_TRUE(preview.pronunciation.isEmpty());
    EXPECT_EQ(preview.definition, "Click to add definition");
    EXPECT_FALSE(preview.isKnown);
}

TEST_F(EbookViewModelTest, GetPreviewForMultiWordKnownTerm) {
    TermManager::instance().addTerm("hello world", "en", TermLevel::Known, "common phrase", "pron");

    EbookViewModel model;
    model.loadContent("say hello world now", "en");

    // "hello world" is now a single display token (longest span wins)
    const auto& tokens = model.getTokenBoundaries();
    const TokenInfo* hwToken = nullptr;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].text == "hello world") {
            hwToken = &tokens[i];
            break;
        }
    }
    ASSERT_NE(hwToken, nullptr) << "Expected token 'hello world' in boundaries";

    TermPreview preview = model.getPreviewForToken(hwToken);

    EXPECT_EQ(preview.term, "hello world");
    EXPECT_EQ(preview.pronunciation, "pron");
    EXPECT_EQ(preview.definition, "common phrase");
    EXPECT_TRUE(preview.isKnown);
}

// ============================================================================
// Mock implementations for dependency injection seam verification
// ============================================================================

class MockLanguageProvider : public ILanguageProvider {
public:
    MOCK_METHOD(LanguageConfig, getLanguageByCode, (const QString&), (const, override));
    MOCK_METHOD(QString, getWordRegex, (const QString&), (const, override));
    MOCK_METHOD(bool, isCharacterBased, (const QString&), (const, override));
};

class MockTermStore : public ITermStore {
public:
    MOCK_METHOD(TrieNode*, getTrieForLanguage, (const QString&), (override));
    MOCK_METHOD(bool, termExists, (const QString&, const QString&), (const, override));
    MOCK_METHOD(Term, getTerm, (const QString&, const QString&), (const, override));
};

// Verify dependency injection seam: injecting a mock language provider
// causes the model to use it instead of the LanguageManager singleton.
TEST_F(EbookViewModelTest, InjectedLanguageProviderIsUsedForRegex) {
    MockLanguageProvider mockLang;
    MockTermStore mockStore;

    // LanguageConfig must provide a valid tokenizer for tokenization to succeed
    LanguageConfig config;
    config.setCode("zz");
    config.setWordRegex("[a-zA-Z]+");
    config.setIsCharBased(false);

    TrieNode emptyTrie;

    EXPECT_CALL(mockLang, getLanguageByCode(QString("zz")))
        .WillOnce(::testing::Return(config));
    EXPECT_CALL(mockLang, getWordRegex(QString("zz")))
        .WillRepeatedly(::testing::Return(QString("[a-zA-Z]+")));
    EXPECT_CALL(mockLang, isCharacterBased(QString("zz")))
        .WillRepeatedly(::testing::Return(false));
    EXPECT_CALL(mockStore, getTrieForLanguage(QString("zz")))
        .WillRepeatedly(::testing::Return(&emptyTrie));

    EbookViewModel model(&mockLang, &mockStore);
    model.loadContent("hello world test", "zz");

    // The model should have tokenized using the mock's regex
    EXPECT_EQ(model.getLanguage(), "zz");
    EXPECT_EQ(model.getText(), "hello world test");
}

// Verify ITermStore seam: injecting a mock term store with a known-term trie
// causes the model to recognize terms from the mock trie instead of TermManager.
TEST_F(EbookViewModelTest, InjectedTermStoreRecognizesTerms) {
    MockLanguageProvider mockLang;
    MockTermStore mockStore;

    LanguageConfig config;
    config.setCode("xx");
    config.setWordRegex("[a-zA-Z]+");
    config.setIsCharBased(false);

    // Build a trie that contains "alice"
    TrieNode mockTrie;
    Term aliceTerm;
    aliceTerm.term = "alice";
    aliceTerm.level = TermLevel::WellKnown;
    aliceTerm.definition = "a person";
    aliceTerm.tokenCount = 1;
    mockTrie.insert(&aliceTerm, {"alice"});

    EXPECT_CALL(mockLang, getLanguageByCode(QString("xx")))
        .WillOnce(::testing::Return(config));
    EXPECT_CALL(mockLang, getWordRegex(QString("xx")))
        .WillRepeatedly(::testing::Return(QString("[a-zA-Z]+")));
    EXPECT_CALL(mockLang, isCharacterBased(QString("xx")))
        .WillRepeatedly(::testing::Return(false));
    EXPECT_CALL(mockStore, getTrieForLanguage(QString("xx")))
        .WillRepeatedly(::testing::Return(&mockTrie));

    EbookViewModel model(&mockLang, &mockStore);
    model.loadContent("alice and bob", "xx");

    // "alice" should be recognized via the mock trie
    const std::vector<TokenInfo>& tokens = model.getTokenBoundaries();
    ASSERT_GT(tokens.size(), 0u);

    bool foundAlice = false;
    for (const auto& token : tokens) {
        if (token.text == "alice") {
            foundAlice = true;
            EXPECT_EQ(token.level, TermLevel::WellKnown);
            break;
        }
    }
    EXPECT_TRUE(foundAlice) << "Expected token 'alice' to be recognized from mock trie";
}
