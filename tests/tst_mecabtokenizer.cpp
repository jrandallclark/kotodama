#include <gtest/gtest.h>
#include "kotodama/mecabtokenizer.h"
#include <QString>

TEST(MeCabTokenizerTest, NameIsMecab)
{
    MeCabTokenizer tokenizer;
    EXPECT_EQ(tokenizer.name(), "mecab");
}

TEST(MeCabTokenizerTest, UnavailableReturnsEmpty)
{
    MeCabTokenizer tokenizer;
    if (tokenizer.isAvailable()) {
        GTEST_SKIP() << "MeCab is installed — skipping unavailable stub test";
    }
    EXPECT_FALSE(tokenizer.isAvailable());
    EXPECT_TRUE(tokenizer.tokenize("test").empty());
    EXPECT_TRUE(tokenizer.tokenize("こんにちは").empty());
}

TEST(MeCabTokenizerTest, AvailableOnSystem)
{
    MeCabTokenizer tokenizer;
    if (!tokenizer.isAvailable()) {
        GTEST_SKIP() << "MeCab not installed";
    }

    auto tokens = tokenizer.tokenize("太郎は本を読んだ");
    EXPECT_GT(tokens.size(), 3u);

    // Positions must be monotonically non-decreasing and non-overlapping
    for (size_t i = 1; i < tokens.size(); ++i) {
        EXPECT_GE(tokens[i].startPos, tokens[i-1].endPos);
    }

    // First token starts at or near 0
    EXPECT_EQ(tokens.front().startPos, 0);
}

TEST(MeCabTokenizerTest, AvailablePositionsCoverInput)
{
    MeCabTokenizer tokenizer;
    if (!tokenizer.isAvailable()) {
        GTEST_SKIP() << "MeCab not installed";
    }

    QString input = "日本語の形態素解析";
    auto tokens = tokenizer.tokenize(input);
    ASSERT_FALSE(tokens.empty());

    // Last token ends at or before the string length
    EXPECT_LE(tokens.back().endPos, input.length());
}

TEST(MeCabTokenizerTest, AvailableEmptyStringReturnsEmpty)
{
    MeCabTokenizer tokenizer;
    if (!tokenizer.isAvailable()) {
        GTEST_SKIP() << "MeCab not installed";
    }
    EXPECT_TRUE(tokenizer.tokenize("").empty());
}

// ============================================================================
// Punctuation and whitespace filtering
// ============================================================================

TEST(MeCabTokenizerTest, PunctuationNotInTokens)
{
    MeCabTokenizer tokenizer;
    if (!tokenizer.isAvailable()) {
        GTEST_SKIP() << "MeCab not installed";
    }

    // 。「」、〟 are Japanese punctuation — all must be filtered
    auto tokens = tokenizer.tokenize("こんにちは。さようなら。");

    for (const auto& token : tokens) {
        QString text = QString::fromStdString(token.text);
        bool hasWordChar = false;
        for (const QChar& ch : text) {
            if (ch.isLetter() || ch.isDigit()) { hasWordChar = true; break; }
        }
        EXPECT_TRUE(hasWordChar) << "Non-word token leaked: " << token.text;
    }
}

TEST(MeCabTokenizerTest, DoubleQuotePunctuationFiltered)
{
    MeCabTokenizer tokenizer;
    if (!tokenizer.isAvailable()) {
        GTEST_SKIP() << "MeCab not installed";
    }

    // 〟 (U+301F) and 。 were the specific chars reported as leaking through
    auto tokens = tokenizer.tokenize("〟太郎は読んだ。");

    EXPECT_GT(tokens.size(), 0u);
    for (const auto& token : tokens) {
        QString text = QString::fromStdString(token.text);
        EXPECT_NE(text, QString("〟")) << "〟 leaked into tokens";
        EXPECT_NE(text, QString("。")) << "。leaked into tokens";
    }
}

TEST(MeCabTokenizerTest, WhitespaceNotInTokens)
{
    MeCabTokenizer tokenizer;
    if (!tokenizer.isAvailable()) {
        GTEST_SKIP() << "MeCab not installed";
    }

    auto tokens = tokenizer.tokenize("こんにちは さようなら");

    for (const auto& token : tokens) {
        QString text = QString::fromStdString(token.text);
        EXPECT_FALSE(text.trimmed().isEmpty())
            << "Whitespace-only token leaked: '" << token.text << "'";
    }
}

// ============================================================================
// Position accuracy — positions must be Unicode char offsets, not byte offsets
// ============================================================================

TEST(MeCabTokenizerTest, PositionsAreCharOffsets)
{
    MeCabTokenizer tokenizer;
    if (!tokenizer.isAvailable()) {
        GTEST_SKIP() << "MeCab not installed";
    }

    // "あいう" — 3 chars, each 3 bytes in UTF-8.
    // Byte offsets would give endPos=9; char offsets give endPos=3.
    auto tokens = tokenizer.tokenize("あいう");
    ASSERT_FALSE(tokens.empty());

    // Combined end must equal the number of Unicode code points (3), not bytes (9)
    EXPECT_LE(tokens.back().endPos, 3)
        << "endPos looks like a byte offset instead of a char offset";
}

TEST(MeCabTokenizerTest, PositionsDoNotExceedInputLength)
{
    MeCabTokenizer tokenizer;
    if (!tokenizer.isAvailable()) {
        GTEST_SKIP() << "MeCab not installed";
    }

    QString input = "日本語の形態素解析テスト";
    auto tokens = tokenizer.tokenize(input);
    ASSERT_FALSE(tokens.empty());

    for (const auto& token : tokens) {
        EXPECT_GE(token.startPos, 0)   << "startPos negative";
        EXPECT_GT(token.endPos, token.startPos) << "endPos not after startPos";
        EXPECT_LE(token.endPos, input.length()) << "endPos past end of string";
    }
}

// ============================================================================
// Context-sensitivity documentation test
// ============================================================================

TEST(MeCabTokenizerTest, ContextChangesTokenization)
{
    MeCabTokenizer tokenizer;
    if (!tokenizer.isAvailable()) {
        GTEST_SKIP() << "MeCab not installed";
    }

    // MeCab is context-sensitive: the same substring can tokenize differently
    // depending on surrounding text. This test documents that the char-based
    // regex fallback (used for trie building/matching) is necessary for
    // consistent term lookup.
    //
    // "浮かん" in isolation vs. inside "浮かんだ空" may produce different morphemes.
    // We just assert that tokenization doesn't crash and positions are valid.
    auto tokensAlone = tokenizer.tokenize("浮かん");
    auto tokensInContext = tokenizer.tokenize("浮かんだ空");

    for (const auto& t : tokensAlone) {
        EXPECT_GE(t.startPos, 0);
        EXPECT_GT(t.endPos, t.startPos);
        EXPECT_LE(t.endPos, 3);  // "浮かん" = 3 chars
    }
    for (const auto& t : tokensInContext) {
        EXPECT_GE(t.startPos, 0);
        EXPECT_GT(t.endPos, t.startPos);
        EXPECT_LE(t.endPos, 5);  // "浮かんだ空" = 5 chars
    }
}
