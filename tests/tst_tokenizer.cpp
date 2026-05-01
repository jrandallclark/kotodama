#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>
#include "kotodama/tokenizer.h"
#include "kotodama/tokenizerbackend.h"
#include "kotodama/languagemanager.h"

using namespace testing;

// Helper: extract text strings from token results
static std::vector<std::string> texts(const std::vector<TokenResult>& results)
{
    std::vector<std::string> out;
    out.reserve(results.size());
    for (const auto& r : results) out.push_back(r.text);
    return out;
}

// Factory method tests
TEST(TokenizerTest, CreateRegexProducesRegexBackend)
{
    auto tok = Tokenizer::createRegex("[a-zA-Z]+");
    ASSERT_NE(tok, nullptr);
    EXPECT_EQ(tok->backendName(), "regex");
    EXPECT_TRUE(tok->isAvailable());
}

TEST(TokenizerTest, CreateJapaneseReturnsTokenizer)
{
    auto tok = Tokenizer::createJapanese();
    ASSERT_NE(tok, nullptr);
    EXPECT_TRUE(tok->isAvailable());
    EXPECT_EQ(tok->backendName(), "mecab");
}

// Whitespace-based tokenization (using \S+ pattern)
TEST(TokenizerTest, WhitespaceTokenization)
{
    auto tok = Tokenizer::createRegex("\\S+");
    auto results = tok->tokenize("Hello world");

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].text, "Hello");
    EXPECT_EQ(results[1].text, "world");
}

TEST(TokenizerTest, MultipleSpacesBetweenWords)
{
    auto tok = Tokenizer::createRegex("\\S+");
    auto results = tok->tokenize("Hello    world");

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].text, "Hello");
    EXPECT_EQ(results[1].text, "world");
}

TEST(TokenizerTest, TabsAndNewlines)
{
    auto tok = Tokenizer::createRegex("\\S+");
    auto results = tok->tokenize("Hello\tworld\ntest");

    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].text, "Hello");
    EXPECT_EQ(results[1].text, "world");
    EXPECT_EQ(results[2].text, "test");
}

// Edge cases
TEST(TokenizerTest, EmptyString)
{
    auto tok = Tokenizer::createRegex("[a-zA-Z]+");
    EXPECT_TRUE(tok->tokenize("").empty());
}

TEST(TokenizerTest, OnlyWhitespace)
{
    auto tok = Tokenizer::createRegex("[a-zA-Z]+");
    EXPECT_TRUE(tok->tokenize("   \t\n  ").empty());
}

TEST(TokenizerTest, SingleWord)
{
    auto tok = Tokenizer::createRegex("[a-zA-Z]+");
    auto results = tok->tokenize("Hello");

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].text, "Hello");
}

// Language-specific pattern tests
TEST(TokenizerTest, EnglishWordsWithPattern)
{
    QString pattern = LanguageManager::instance().getWordRegex("en");
    auto tok = Tokenizer::createRegex(pattern);
    auto results = tok->tokenize("Hello, world! How's it going?");
    auto tokenTexts = texts(results);

    EXPECT_GT(results.size(), 0u);
    EXPECT_THAT(tokenTexts, Contains("Hello"));
    EXPECT_THAT(tokenTexts, Contains("world"));
    EXPECT_THAT(tokenTexts, Contains("How's"));
}

TEST(TokenizerTest, FrenchWordsWithApostrophes)
{
    QString pattern = LanguageManager::instance().getWordRegex("fr");
    auto tok = Tokenizer::createRegex(pattern);
    auto results = tok->tokenize("l'odeur d'été");
    auto tokenTexts = texts(results);

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].text, "l'odeur");
    EXPECT_EQ(results[1].text, "d'été");
}

TEST(TokenizerTest, FrenchAccentedCharacters)
{
    QString pattern = LanguageManager::instance().getWordRegex("fr");
    auto tok = Tokenizer::createRegex(pattern);
    auto results = tok->tokenize("café résumé naïve");
    auto tokenTexts = texts(results);

    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].text, "café");
    EXPECT_EQ(results[1].text, "résumé");
    EXPECT_EQ(results[2].text, "naïve");
}

TEST(TokenizerTest, EnglishHyphenatedWords)
{
    QString pattern = LanguageManager::instance().getWordRegex("en");
    auto tok = Tokenizer::createRegex(pattern);
    auto results = tok->tokenize("well-known state-of-the-art");
    auto tokenTexts = texts(results);

    EXPECT_THAT(tokenTexts, Contains("well-known"));
    EXPECT_THAT(tokenTexts, Contains("state-of-the-art"));
}

// Special characters and punctuation
TEST(TokenizerTest, WordsWithPunctuation)
{
    QString pattern = LanguageManager::instance().getWordRegex("en");
    auto tok = Tokenizer::createRegex(pattern);
    auto results = tok->tokenize("Hello, world! Test.");
    auto tokenTexts = texts(results);

    EXPECT_THAT(tokenTexts, Contains("Hello"));
    EXPECT_THAT(tokenTexts, Contains("world"));
    EXPECT_THAT(tokenTexts, Contains("Test"));
    EXPECT_THAT(tokenTexts, Not(Contains(",")));
    EXPECT_THAT(tokenTexts, Not(Contains("!")));
    EXPECT_THAT(tokenTexts, Not(Contains(".")));
}

TEST(TokenizerTest, NumbersAreNotWords)
{
    QString pattern = LanguageManager::instance().getWordRegex("en");
    auto tok = Tokenizer::createRegex(pattern);
    auto results = tok->tokenize("I have 123 apples and 456 oranges");
    auto tokenTexts = texts(results);

    EXPECT_THAT(tokenTexts, Contains("have"));
    EXPECT_THAT(tokenTexts, Contains("apples"));
    EXPECT_THAT(tokenTexts, Contains("and"));
    EXPECT_THAT(tokenTexts, Contains("oranges"));
    EXPECT_THAT(tokenTexts, Not(Contains("123")));
    EXPECT_THAT(tokenTexts, Not(Contains("456")));
}

// Character splitting tests
TEST(TokenizerTest, SplitIntoCharactersFalseByDefault)
{
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    auto tok = Tokenizer::createRegex(pattern);
    auto results = tok->tokenize("こんにちは");

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].text, "こんにちは");
}

TEST(TokenizerTest, SplitIntoCharactersTrue)
{
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    auto tok = Tokenizer::createRegex(pattern, true);
    auto results = tok->tokenize("こんにちは");

    ASSERT_EQ(results.size(), 5u);
    // Positions must be consecutive
    for (size_t i = 0; i < results.size(); ++i) {
        EXPECT_EQ(results[i].startPos, static_cast<int>(i));
        EXPECT_EQ(results[i].endPos,   static_cast<int>(i) + 1);
    }
}

TEST(TokenizerTest, SplitIntoCharactersWithMixedScript)
{
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    auto tok = Tokenizer::createRegex(pattern, true);
    auto results = tok->tokenize("日本語");

    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].startPos, 0);
    EXPECT_EQ(results[2].endPos,   3);
}

TEST(TokenizerTest, SplitIntoCharactersWithPunctuation)
{
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    auto tok = Tokenizer::createRegex(pattern, true);
    auto results = tok->tokenize("こんにちは。");

    // Punctuation (。) is outside the Japanese regex range — not matched
    ASSERT_EQ(results.size(), 5u);
}

TEST(TokenizerTest, SplitIntoCharactersMixedLanguages)
{
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    auto tok = Tokenizer::createRegex(pattern, true);
    auto results = tok->tokenize("こんにちは world さようなら");

    ASSERT_EQ(results.size(), 10u);  // 5 + 5 characters
    EXPECT_EQ(results[0].startPos, 0);
    EXPECT_EQ(results[4].endPos,   5);
}

// Position integrity
TEST(TokenizerTest, PositionsNeverOverlap)
{
    QString pattern = LanguageManager::instance().getWordRegex("en");
    auto tok = Tokenizer::createRegex(pattern);
    auto results = tok->tokenize("The quick brown fox jumps over the lazy dog");

    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_GE(results[i].startPos, results[i-1].endPos);
    }
}

// Japanese factory
TEST(TokenizerTest, JapaneseFactoryTokenizesHiragana)
{
    auto tok = Tokenizer::createJapanese();
    auto results = tok->tokenize("こんにちは");

    EXPECT_GT(results.size(), 0u);
    // Each result must have valid bounds
    for (const auto& r : results) {
        EXPECT_GE(r.startPos, 0);
        EXPECT_GT(r.endPos, r.startPos);
    }
}

// ============================================================================
// Japanese char-based regex edge cases
// ============================================================================

// 々 (U+3005) — ideographic iteration mark — must be in the Japanese pattern
TEST(TokenizerTest, JapaneseIterationMarkIncluded)
{
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    auto tok = Tokenizer::createRegex(pattern, true);
    auto results = tok->tokenize("様々");

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].text, "様");
    EXPECT_EQ(results[1].text, "々");
}

// Each Japanese char must map to consecutive Unicode char offsets (not byte offsets)
TEST(TokenizerTest, JapaneseCharBasedPositionsAreCharUnits)
{
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    auto tok = Tokenizer::createRegex(pattern, true);
    auto results = tok->tokenize("日本語");

    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].startPos, 0);
    EXPECT_EQ(results[0].endPos,   1);
    EXPECT_EQ(results[1].startPos, 1);
    EXPECT_EQ(results[1].endPos,   2);
    EXPECT_EQ(results[2].startPos, 2);
    EXPECT_EQ(results[2].endPos,   3);
}

// Japanese punctuation (。、〟「」) is outside the script ranges — must be excluded
TEST(TokenizerTest, JapaneseCharBasedPunctuationExcluded)
{
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    auto tok = Tokenizer::createRegex(pattern, true);

    // 5 hiragana + 。 + 5 hiragana + 、 → 10 word tokens
    auto results = tok->tokenize("こんにちは。さようなら、");
    EXPECT_EQ(results.size(), 10u);
}

// createJapanese() (MeCab) must exclude punctuation
TEST(TokenizerTest, JapaneseFactoryPunctuationFiltered)
{
    auto tok = Tokenizer::createJapanese();
    auto results = tok->tokenize("こんにちは。さようなら。");

    for (const auto& r : results) {
        QString text = QString::fromStdString(r.text);
        bool hasWordChar = false;
        for (const QChar& ch : text) {
            if (ch.isLetter() || ch.isDigit()) { hasWordChar = true; break; }
        }
        EXPECT_TRUE(hasWordChar) << "Non-word token in output: " << r.text;
    }
}

// Katakana (テスト, U+30A0–U+30FF) must be in the Japanese regex
TEST(TokenizerTest, JapaneseCharBasedKatakanaIncluded)
{
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    auto tok = Tokenizer::createRegex(pattern, true);
    auto results = tok->tokenize("テスト");

    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].text, "テ");
    EXPECT_EQ(results[1].text, "ス");
    EXPECT_EQ(results[2].text, "ト");
}

// Kanji + hiragana mixed string — positions must be contiguous char units
TEST(TokenizerTest, JapaneseMixedScriptConsecutivePositions)
{
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    auto tok = Tokenizer::createRegex(pattern, true);
    auto results = tok->tokenize("空を飛ぶ");  // 4 chars

    ASSERT_EQ(results.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(results[i].startPos, i);
        EXPECT_EQ(results[i].endPos,   i + 1);
    }
}

TEST(TokenizerTest, JapaneseFactoryPositionsDoNotExceedLength)
{
    auto tok = Tokenizer::createJapanese();
    QString input = "日本語テスト";
    auto results = tok->tokenize(input);

    ASSERT_FALSE(results.empty());
    EXPECT_LE(results.back().endPos, input.length());
    EXPECT_GE(results.front().startPos, 0);
}
