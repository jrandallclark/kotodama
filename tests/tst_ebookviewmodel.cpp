#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "kotodama/ebookviewmodel.h"
#include "kotodama/termmanager.h"
#include "kotodama/databasemanager.h"

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

    Term* found = model.findTermAtPosition(7);  // Inside "world"

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "world");
}

TEST_F(EbookViewModelTest, FindTermAtPositionReturnsNullForNonTerm) {
    TermManager::instance().addTerm("world", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("hello world test", "en");

    Term* found = model.findTermAtPosition(0);  // "hello" is not a term

    EXPECT_EQ(found, nullptr);
}

TEST_F(EbookViewModelTest, FindTermAtPositionWorksForMultiWord) {
    TermManager::instance().addTerm("hello world", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("The hello world is here", "en");

    // Position 5 is inside "hello" of the multi-word term
    Term* found1 = model.findTermAtPosition(5);
    // Position 8 is inside "world" of the multi-word term
    Term* found2 = model.findTermAtPosition(8);

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

    Term* at3 = model.findTermAtPosition(3);  // 日
    Term* at4 = model.findTermAtPosition(4);  // 本
    Term* at5 = model.findTermAtPosition(5);  // 語
    Term* at2 = model.findTermAtPosition(2);  // は — not part of the term
    Term* at6 = model.findTermAtPosition(6);  // で — not part of the term

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
    Term* atの = model.findTermAtPosition(4);
    ASSERT_NE(atの, nullptr);
    EXPECT_EQ(atの->term, "のぞい");

    // Hovering over 双眼鏡 range must return 双眼鏡
    Term* at双 = model.findTermAtPosition(1);
    ASSERT_NE(at双, nullptr);
    EXPECT_EQ(at双->term, "双眼鏡");

    // Hovering over で (between the two terms) must return nullptr
    Term* atで = model.findTermAtPosition(3);
    EXPECT_EQ(atで, nullptr);
}

// ============================================================================
// MeCab display-token / trie-term overlap edge case
//
// Scenario:  聞こ is a known term (chars [4,6)).
//            MeCab may parse 聞こえ as a single morpheme [4,7) because
//            聞こえる (to be heard) is one verb.  The display token [4,7)
//            extends PAST the known term boundary at 6, so the tokenUsed
//            check (endPos <= term.endPos) fails and the whole token is
//            emitted as an unknown highlight — masking the known term.
//
// Rules that must hold regardless of which tokenizer handles display:
//   1. Trie matching (char-based): 聞こ matched at [4,6); え at 6 is unknown.
//   2. findTermAtPosition: returns 聞こ for positions 4 and 5; null for 6.
//   3. A known highlight for [4,6) must exist in getHighlights().
//   4. No unknown highlight may start before position 6 (i.e. no unknown
//      region may overlap the known term's range [4,6)).
// ============================================================================

TEST_F(EbookViewModelTest, JapaneseKnownTermNotMaskedByMeCabToken) {
    // 聞こ is known; 聞こえ is not in the database
    TermManager::instance().addTerm("聞こ", "ja", TermLevel::Known);

    EbookViewModel model;
    // 水(0)の(1)音(2)が(3)聞(4)こ(5)え(6)た(7)
    model.loadContent("水の音が聞こえた", "ja");

    // --- Rule 1: trie matched 聞こ at [4,6), not 聞こえ ---
    const auto& termPositions = model.getTermPositions();
    ASSERT_EQ(termPositions.size(), 1u);
    EXPECT_EQ(termPositions[0].termData.term, "聞こ");
    EXPECT_EQ(termPositions[0].startPos, 4);
    EXPECT_EQ(termPositions[0].endPos,   6);

    // --- Rule 2: hover positions ---
    Term* at4 = model.findTermAtPosition(4);  // 聞 — inside the term
    Term* at5 = model.findTermAtPosition(5);  // こ — inside the term
    Term* at6 = model.findTermAtPosition(6);  // え — NOT inside any term

    ASSERT_NE(at4, nullptr);
    ASSERT_NE(at5, nullptr);
    EXPECT_EQ(at4->term, "聞こ");
    EXPECT_EQ(at5->term, "聞こ");
    EXPECT_EQ(at6, nullptr) << "え is not part of 聞こ; hover must return null";

    // --- Rules 3 & 4: highlight correctness ---
    auto highlights = model.getHighlights();

    // Rule 3: known highlight for the term must exist
    auto knownIt = std::find_if(highlights.begin(), highlights.end(),
        [](const HighlightInfo& h) {
            return !h.isUnknown && h.startPos == 4 && h.endPos == 6;
        });
    EXPECT_NE(knownIt, highlights.end())
        << "Known highlight for 聞こ at [4,6) not found";

    // Rule 4: no unknown highlight may overlap the known term's range [4,6).
    // If MeCab returns 聞こえ as a single token [4,7) and it leaks through as
    // unknown, this assertion will fail — revealing the display-layer bug.
    for (const auto& h : highlights) {
        if (h.isUnknown) {
            bool overlapsKnownTerm = (h.startPos < 6 && h.endPos > 4);
            EXPECT_FALSE(overlapsKnownTerm)
                << "Unknown highlight [" << h.startPos << "," << h.endPos
                << ") overlaps known term [4,6) — MeCab token boundary mismatch";
        }
    }
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
    TermManager::instance().addTerm("hello world", "en", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("say hello world now", "en");

    // "world" is token index 2, also part of "hello world" term [4, 15)
    QPair<int, int> range = model.getFocusRange(2);

    EXPECT_EQ(range.first, 4);
    EXPECT_EQ(range.second, 15);
}

TEST_F(EbookViewModelTest, GetFocusRange_JapaneseMultiCharTermReturnsFullSpan) {
    TermManager::instance().addTerm("した", "ja", TermLevel::Known);

    EbookViewModel model;
    model.loadContent("にした際", "ja");

    // Find which token index corresponds to "し"
    const auto& tokens = model.getTokenBoundaries();
    int shiIndex = -1;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].text == "し") {
            shiIndex = static_cast<int>(i);
            break;
        }
    }

    ASSERT_GE(shiIndex, 0) << "Could not find token 'し'";

    // "し" is part of "した" term - should return full term span
    QPair<int, int> range = model.getFocusRange(shiIndex);

    // The term "した" should span both characters
    EXPECT_EQ(range.second - range.first, 2)
        << "Focus range for 'し' should span 2 characters (the full 'した' term)";
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

    // Find a token within the multi-word term
    const auto& tokens = model.getTokenBoundaries();
    ASSERT_GE(tokens.size(), 3u);
    // "hello" should be at index 1, within the "hello world" term
    const TokenInfo* token = &tokens[1];

    TermPreview preview = model.getPreviewForToken(token);

    EXPECT_EQ(preview.term, "hello world");
    EXPECT_EQ(preview.pronunciation, "pron");
    EXPECT_EQ(preview.definition, "common phrase");
    EXPECT_TRUE(preview.isKnown);
}
