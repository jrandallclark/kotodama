#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>
#include "kotodama/regextokenizer.h"

using namespace testing;

// Helper: extract text strings from token results
static std::vector<std::string> texts(const std::vector<TokenResult>& results)
{
    std::vector<std::string> out;
    out.reserve(results.size());
    for (const auto& r : results) out.push_back(r.text);
    return out;
}

// Basic word tokenization
TEST(RegexTokenizerTest, BasicWords)
{
    RegexTokenizer tok("[a-zA-Z]+");
    auto results = tok.tokenize("hello world");

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].text, "hello");
    EXPECT_EQ(results[1].text, "world");
}

TEST(RegexTokenizerTest, EmptyStringReturnsEmpty)
{
    RegexTokenizer tok("[a-zA-Z]+");
    EXPECT_TRUE(tok.tokenize("").empty());
}

TEST(RegexTokenizerTest, NoMatchReturnsEmpty)
{
    RegexTokenizer tok("[a-zA-Z]+");
    EXPECT_TRUE(tok.tokenize("123 456").empty());
}

// Position tracking
TEST(RegexTokenizerTest, PositionsAreCorrect)
{
    RegexTokenizer tok("[a-zA-Z]+");
    auto results = tok.tokenize("hello world");

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].startPos, 0);
    EXPECT_EQ(results[0].endPos,   5);
    EXPECT_EQ(results[1].startPos, 6);
    EXPECT_EQ(results[1].endPos,  11);
}

TEST(RegexTokenizerTest, PositionsWithLeadingWhitespace)
{
    RegexTokenizer tok("[a-zA-Z]+");
    auto results = tok.tokenize("  hello");

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].startPos, 2);
    EXPECT_EQ(results[0].endPos,   7);
}

TEST(RegexTokenizerTest, PositionsAreMonotonicallyIncreasing)
{
    RegexTokenizer tok("[a-zA-Z]+");
    auto results = tok.tokenize("the quick brown fox");

    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_GE(results[i].startPos, results[i-1].endPos);
    }
}

// Character splitting
TEST(RegexTokenizerTest, SplitIntoCharsFalseByDefault)
{
    RegexTokenizer tok("\\S+");
    auto results = tok.tokenize("hello");

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].text, "hello");
}

TEST(RegexTokenizerTest, SplitIntoCharsTrue)
{
    RegexTokenizer tok("[\\x{3040}-\\x{309F}]+", true);
    auto results = tok.tokenize("こんにちは");

    ASSERT_EQ(results.size(), 5u);
    EXPECT_EQ(results[0].text, "\xe3\x81\x93");  // こ
    // Positions must be consecutive
    for (size_t i = 0; i < results.size(); ++i) {
        EXPECT_EQ(results[i].startPos, static_cast<int>(i));
        EXPECT_EQ(results[i].endPos,   static_cast<int>(i) + 1);
    }
}

TEST(RegexTokenizerTest, SplitIntoCharsKanji)
{
    RegexTokenizer tok("[\\x{4E00}-\\x{9FFF}]+", true);
    auto results = tok.tokenize("日本語");

    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].startPos, 0);
    EXPECT_EQ(results[0].endPos,   1);
    EXPECT_EQ(results[1].startPos, 1);
    EXPECT_EQ(results[1].endPos,   2);
    EXPECT_EQ(results[2].startPos, 2);
    EXPECT_EQ(results[2].endPos,   3);
}

TEST(RegexTokenizerTest, SplitIntoCharsPunctuationExcluded)
{
    RegexTokenizer tok("[\\x{3040}-\\x{309F}]+", true);
    auto results = tok.tokenize("こんにちは。");

    // Punctuation is outside the regex, so not matched
    ASSERT_EQ(results.size(), 5u);
}

// Unicode / accented characters
TEST(RegexTokenizerTest, AccentedCharacters)
{
    RegexTokenizer tok("[a-zA-Z\\x{00C0}-\\x{024F}]+");
    auto results = tok.tokenize("café résumé");

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].text, "café");
    EXPECT_EQ(results[1].text, "résumé");
}

TEST(RegexTokenizerTest, MixedScriptOnlyMatchesPattern)
{
    RegexTokenizer tok("[\\x{3040}-\\x{309F}\\x{4E00}-\\x{9FFF}]+", true);
    auto results = tok.tokenize("こんにちは world さようなら");

    // "world" is outside the Japanese ranges — not matched
    ASSERT_EQ(results.size(), 10u);  // 5 hiragana + 5 hiragana
}

// Name and availability
TEST(RegexTokenizerTest, NameIsRegex)
{
    RegexTokenizer tok("[a-z]+");
    EXPECT_EQ(tok.name(), "regex");
}

TEST(RegexTokenizerTest, IsAlwaysAvailable)
{
    RegexTokenizer tok("[a-z]+");
    EXPECT_TRUE(tok.isAvailable());
}
