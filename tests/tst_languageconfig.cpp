#include <gtest/gtest.h>
#include <QRegularExpression>
#include "kotodama/languageconfig.h"
#include "kotodama/languagemanager.h"
#include "kotodama/tokenizer.h"

// Note: LanguageConfig uses a database via LanguageManager
// In a production test suite, you'd want to use a test database or mock the DB layer

class LanguageConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any custom languages from previous tests
        // Built-in languages (en, fr, ja) cannot be deleted
    }

    void TearDown() override {
        // Clean up after each test
        // Note: In future, we might want to delete test custom languages here
    }

    // Helper to test if a regex matches/rejects expected inputs
    void testRegexPattern(const QString& pattern,
                         const QStringList& shouldMatch,
                         const QStringList& shouldNotMatch) {
        QRegularExpression regex(pattern);
        ASSERT_TRUE(regex.isValid()) << "Regex pattern is invalid: "
                                     << pattern.toStdString();

        for (const QString& text : shouldMatch) {
            QRegularExpressionMatch match = regex.match(text);
            EXPECT_TRUE(match.hasMatch())
                << "Expected '" << text.toStdString() << "' to match pattern";
        }

        for (const QString& text : shouldNotMatch) {
            // For "should not match", we check if the pattern extracts ONLY the text
            // If it doesn't extract the whole thing, it means it rejected some characters
            QRegularExpressionMatchIterator it = regex.globalMatch(text);
            QString extracted;
            while (it.hasNext()) {
                extracted += it.next().captured(0);
            }
            EXPECT_NE(extracted, text)
                << "Expected '" << text.toStdString() << "' to NOT fully match pattern";
        }
    }
};

// Default Regex Tests
TEST_F(LanguageConfigTest, GetDefaultEnglishRegex) {
    QString regex = LanguageManager::instance().getWordRegex("en");

    EXPECT_FALSE(regex.isEmpty());
    EXPECT_TRUE(regex.contains("a-zA-Z"));
}

TEST_F(LanguageConfigTest, GetDefaultFrenchRegex) {
    QString regex = LanguageManager::instance().getWordRegex("fr");

    EXPECT_FALSE(regex.isEmpty());
    EXPECT_TRUE(regex.contains("a-zA-Z"));
    // Should include Unicode ranges for accented characters
    EXPECT_TRUE(regex.contains("\\x{"));
}

TEST_F(LanguageConfigTest, GetDefaultJapaneseRegex) {
    QString regex = LanguageManager::instance().getWordRegex("ja");

    EXPECT_FALSE(regex.isEmpty());
    // Should include Unicode ranges for Hiragana, Katakana, and Kanji
    EXPECT_TRUE(regex.contains("\\x{3040}"));  // Hiragana start
    EXPECT_TRUE(regex.contains("\\x{30A0}"));  // Katakana start
    EXPECT_TRUE(regex.contains("\\x{4E00}"));  // Kanji start
}

TEST_F(LanguageConfigTest, UnknownLanguageFallback) {
    QString regex = LanguageManager::instance().getWordRegex("unknown-lang");

    // Should return a fallback pattern
    EXPECT_FALSE(regex.isEmpty());
    EXPECT_TRUE(regex.contains("a-zA-Z"));
}

// English Regex Pattern Tests
TEST_F(LanguageConfigTest, EnglishRegexMatchesBasicWords) {
    QString pattern = LanguageManager::instance().getWordRegex("en");

    QStringList shouldMatch = {
        "hello",
        "world",
        "Hello",
        "WORLD",
        "test"
    };

    QStringList shouldNotMatch = {
        "123",          // Numbers
        "hello123",     // Mixed
        "test@email"    // Special chars
    };

    testRegexPattern(pattern, shouldMatch, shouldNotMatch);
}

TEST_F(LanguageConfigTest, EnglishRegexMatchesContractions) {
    QString pattern = LanguageManager::instance().getWordRegex("en");
    QRegularExpression regex(pattern);

    // Test apostrophes in contractions
    EXPECT_TRUE(regex.match("don't").hasMatch());
    EXPECT_TRUE(regex.match("it's").hasMatch());
    EXPECT_TRUE(regex.match("we'll").hasMatch());
    EXPECT_TRUE(regex.match("I'm").hasMatch());
}

TEST_F(LanguageConfigTest, EnglishRegexMatchesHyphenatedWords) {
    QString pattern = LanguageManager::instance().getWordRegex("en");
    QRegularExpression regex(pattern);

    EXPECT_TRUE(regex.match("well-known").hasMatch());
    EXPECT_TRUE(regex.match("state-of-the-art").hasMatch());
    EXPECT_TRUE(regex.match("twenty-one").hasMatch());
}

// French Regex Pattern Tests
TEST_F(LanguageConfigTest, FrenchRegexMatchesAccentedCharacters) {
    QString pattern = LanguageManager::instance().getWordRegex("fr");
    QRegularExpression regex(pattern);

    // Common French accented characters
    EXPECT_TRUE(regex.match("café").hasMatch());
    EXPECT_TRUE(regex.match("résumé").hasMatch());
    EXPECT_TRUE(regex.match("naïve").hasMatch());
    EXPECT_TRUE(regex.match("être").hasMatch());
    EXPECT_TRUE(regex.match("où").hasMatch());
    EXPECT_TRUE(regex.match("français").hasMatch());
}

TEST_F(LanguageConfigTest, FrenchRegexMatchesStraightApostrophe) {
    QString pattern = LanguageManager::instance().getWordRegex("fr");
    QRegularExpression regex(pattern);

    // Straight apostrophe (')
    EXPECT_TRUE(regex.match("l'odeur").hasMatch());
    EXPECT_TRUE(regex.match("d'été").hasMatch());
    EXPECT_TRUE(regex.match("qu'est-ce").hasMatch());
}

TEST_F(LanguageConfigTest, FrenchRegexMatchesCurlyApostrophe) {
    QString pattern = LanguageManager::instance().getWordRegex("fr");
    QRegularExpression regex(pattern);

    // Curly apostrophe (') - \x{2019}
    EXPECT_TRUE(regex.match("l'odeur").hasMatch());
    EXPECT_TRUE(regex.match("d'été").hasMatch());
    EXPECT_TRUE(regex.match("aujourd'hui").hasMatch());
}

TEST_F(LanguageConfigTest, FrenchRegexExtractsWordsCorrectly) {
    QString pattern = LanguageManager::instance().getWordRegex("fr");
    QRegularExpression regex(pattern);

    QString text = "Bonjour, c'est l'été à Paris!";
    QRegularExpressionMatchIterator it = regex.globalMatch(text);

    QStringList matches;
    while (it.hasNext()) {
        matches << it.next().captured(0);
    }

    EXPECT_EQ(matches.size(), 5);
    EXPECT_TRUE(matches.contains("Bonjour"));
    EXPECT_TRUE(matches.contains("c'est"));
    EXPECT_TRUE(matches.contains("l'été"));
    EXPECT_TRUE(matches.contains("à"));
    EXPECT_TRUE(matches.contains("Paris"));
}

// Japanese Regex Pattern Tests
TEST_F(LanguageConfigTest, JapaneseRegexMatchesHiragana) {
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    QRegularExpression regex(pattern);

    // Common Hiragana words
    EXPECT_TRUE(regex.match("こんにちは").hasMatch());  // konnichiwa (hello)
    EXPECT_TRUE(regex.match("ありがとう").hasMatch());  // arigatou (thank you)
    EXPECT_TRUE(regex.match("さようなら").hasMatch());  // sayounara (goodbye)
}

TEST_F(LanguageConfigTest, JapaneseRegexMatchesKatakana) {
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    QRegularExpression regex(pattern);

    // Common Katakana words
    EXPECT_TRUE(regex.match("コンピュータ").hasMatch());  // konpyuuta (computer)
    EXPECT_TRUE(regex.match("カメラ").hasMatch());        // kamera (camera)
    EXPECT_TRUE(regex.match("テスト").hasMatch());        // tesuto (test)
}

TEST_F(LanguageConfigTest, JapaneseRegexMatchesKanji) {
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    QRegularExpression regex(pattern);

    // Common Kanji words
    EXPECT_TRUE(regex.match("日本").hasMatch());    // nihon (Japan)
    EXPECT_TRUE(regex.match("東京").hasMatch());    // toukyou (Tokyo)
    EXPECT_TRUE(regex.match("勉強").hasMatch());    // benkyou (study)
}

TEST_F(LanguageConfigTest, JapaneseRegexMatchesMixedScript) {
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    QRegularExpression regex(pattern);

    // Mixed script (common in Japanese)
    EXPECT_TRUE(regex.match("日本語").hasMatch());      // nihongo (Japanese language)
    EXPECT_TRUE(regex.match("お金").hasMatch());        // okane (money)
    EXPECT_TRUE(regex.match("勉強する").hasMatch());    // benkyou suru (to study)
}

TEST_F(LanguageConfigTest, JapaneseRegexExtractsSequencesCorrectly) {
    QString pattern = LanguageManager::instance().getWordRegex("ja");
    QRegularExpression regex(pattern);

    // Japanese doesn't use spaces, so continuous characters are matched as one sequence
    // Note: The regex extracts sequences, but the tokenizer splits them into individual characters
    QString text = "私は日本語を勉強します。";  // I study Japanese.
    QRegularExpressionMatchIterator it = regex.globalMatch(text);

    QStringList matches;
    while (it.hasNext()) {
        matches << it.next().captured(0);
    }

    // Should extract one continuous sequence (Japanese has no spaces)
    // The punctuation (。) is not part of the Japanese character ranges
    EXPECT_EQ(matches.size(), 1);
    EXPECT_EQ(matches[0], "私は日本語を勉強します");

    // Test with text containing spaces (e.g., mixed with English)
    text = "こんにちは world さようなら";
    it = regex.globalMatch(text);
    matches.clear();
    while (it.hasNext()) {
        matches << it.next().captured(0);
    }

    // Should extract two Japanese sequences separated by the English word
    EXPECT_EQ(matches.size(), 2);
    EXPECT_TRUE(matches.contains("こんにちは"));
    EXPECT_TRUE(matches.contains("さようなら"));
}

// Custom Language Tests
TEST_F(LanguageConfigTest, AddCustomLanguage) {
    LanguageConfig norwegian("Norwegian", "no", "[-'a-zA-ZæøåÆØÅ]+", false, 6);

    EXPECT_TRUE(LanguageManager::instance().addCustomLanguage(norwegian));

    QString retrieved = LanguageManager::instance().getWordRegex("no");
    EXPECT_EQ(retrieved, "[-'a-zA-ZæøåÆØÅ]+");

    // Clean up
    LanguageManager::instance().deleteCustomLanguage("no");
}

TEST_F(LanguageConfigTest, CannotOverrideBuiltInLanguage) {
    LanguageConfig customEnglish("My English", "en", "[0-9]+", false, 6);

    // Should fail because 'en' is a built-in language
    EXPECT_FALSE(LanguageManager::instance().addCustomLanguage(customEnglish));

    // Built-in should remain unchanged
    QString retrieved = LanguageManager::instance().getWordRegex("en");
    EXPECT_TRUE(retrieved.contains("a-zA-Z"));
    EXPECT_FALSE(retrieved == "[0-9]+");
}

TEST_F(LanguageConfigTest, IsBuiltInDetection) {
    EXPECT_TRUE(LanguageManager::instance().isBuiltIn("en"));
    EXPECT_TRUE(LanguageManager::instance().isBuiltIn("fr"));
    EXPECT_TRUE(LanguageManager::instance().isBuiltIn("ja"));
    EXPECT_TRUE(LanguageManager::instance().isBuiltIn("es"));
    EXPECT_FALSE(LanguageManager::instance().isBuiltIn("no"));
    EXPECT_FALSE(LanguageManager::instance().isBuiltIn("unknown"));
}

TEST_F(LanguageConfigTest, UpdateCustomLanguage) {
    // Add a custom language
    LanguageConfig danish("Danish", "da", "[a-zA-ZæøåÆØÅ]+", false, 6);
    EXPECT_TRUE(LanguageManager::instance().addCustomLanguage(danish));

    // Update it
    LanguageConfig updatedDanish("Dansk", "da", "[-'a-zA-ZæøåÆØÅ]+", false, 6);
    EXPECT_TRUE(LanguageManager::instance().updateCustomLanguage("da", updatedDanish));

    // Verify update
    LanguageConfig retrieved = LanguageManager::instance().getLanguageByCode("da");
    EXPECT_EQ(retrieved.name(), "Dansk");
    EXPECT_TRUE(retrieved.wordRegex().contains("æ"));

    // Clean up
    LanguageManager::instance().deleteCustomLanguage("da");
}

TEST_F(LanguageConfigTest, DeleteCustomLanguage) {
    // Add a custom language
    LanguageConfig finnish("Finnish", "fi", "[a-zA-ZäöåÄÖÅ]+", false, 6);
    EXPECT_TRUE(LanguageManager::instance().addCustomLanguage(finnish));

    // Verify it exists
    QString regex = LanguageManager::instance().getWordRegex("fi");
    EXPECT_FALSE(regex.isEmpty());

    // Delete it
    EXPECT_TRUE(LanguageManager::instance().deleteCustomLanguage("fi"));

    // Verify it's gone (should return fallback)
    regex = LanguageManager::instance().getWordRegex("fi");
    EXPECT_TRUE(regex.contains("a-zA-Z")); // Fallback pattern
}

TEST_F(LanguageConfigTest, CannotDeleteBuiltInLanguage) {
    // Attempting to delete built-in should fail
    EXPECT_FALSE(LanguageManager::instance().deleteCustomLanguage("en"));
    EXPECT_FALSE(LanguageManager::instance().deleteCustomLanguage("fr"));
    EXPECT_FALSE(LanguageManager::instance().deleteCustomLanguage("ja"));

    // Verify they still exist
    EXPECT_FALSE(LanguageManager::instance().getWordRegex("en").isEmpty());
}

// Validation Tests
TEST_F(LanguageConfigTest, ValidateLanguageCodeFormat) {
    // Valid codes (2-3 lowercase letters)
    EXPECT_TRUE(LanguageManager::instance().validateLanguageCode("no"));
    EXPECT_TRUE(LanguageManager::instance().validateLanguageCode("da"));
    EXPECT_TRUE(LanguageManager::instance().validateLanguageCode("fi"));

    // Invalid codes
    EXPECT_FALSE(LanguageManager::instance().validateLanguageCode("e"));      // Too short
    EXPECT_FALSE(LanguageManager::instance().validateLanguageCode("engl"));   // Too long
    EXPECT_FALSE(LanguageManager::instance().validateLanguageCode("EN"));     // Uppercase
    EXPECT_FALSE(LanguageManager::instance().validateLanguageCode("e1"));     // Contains number
    EXPECT_FALSE(LanguageManager::instance().validateLanguageCode("e-n"));    // Contains hyphen
}

TEST_F(LanguageConfigTest, ValidateLanguageCodeUniqueness) {
    // Built-in codes should fail validation
    EXPECT_FALSE(LanguageManager::instance().validateLanguageCode("en"));
    EXPECT_FALSE(LanguageManager::instance().validateLanguageCode("fr"));
    EXPECT_FALSE(LanguageManager::instance().validateLanguageCode("ja"));
    EXPECT_FALSE(LanguageManager::instance().validateLanguageCode("es"));

    // New code should pass
    EXPECT_TRUE(LanguageManager::instance().validateLanguageCode("no"));
}

TEST_F(LanguageConfigTest, ValidateRegexPattern) {
    // Valid patterns
    EXPECT_TRUE(LanguageManager::instance().validateRegexPattern("[a-z]+"));
    EXPECT_TRUE(LanguageManager::instance().validateRegexPattern("[-'a-zA-Z]+"));
    EXPECT_TRUE(LanguageManager::instance().validateRegexPattern("[\\x{AC00}-\\x{D7AF}]+"));

    // Invalid patterns
    EXPECT_FALSE(LanguageManager::instance().validateRegexPattern(""));           // Empty
    EXPECT_FALSE(LanguageManager::instance().validateRegexPattern("[a-z"));       // Unclosed bracket
    EXPECT_FALSE(LanguageManager::instance().validateRegexPattern("(abc"));       // Unclosed paren
}

// Regex Validity Tests
TEST_F(LanguageConfigTest, AllDefaultRegexesAreValid) {
    QStringList languages = {"en", "fr", "ja"};

    for (const QString& lang : languages) {
        QString pattern = LanguageManager::instance().getWordRegex(lang);
        QRegularExpression regex(pattern);

        EXPECT_TRUE(regex.isValid())
            << "Default regex for " << lang.toStdString() << " is invalid: "
            << regex.errorString().toStdString();
    }
}

TEST_F(LanguageConfigTest, FallbackRegexIsValid) {
    QString pattern = LanguageManager::instance().getWordRegex("unknown-language");
    QRegularExpression regex(pattern);

    EXPECT_TRUE(regex.isValid())
        << "Fallback regex is invalid: "
        << regex.errorString().toStdString();
}

// Integration Tests
TEST_F(LanguageConfigTest, EnglishRegexRejectsNumbers) {
    QString pattern = LanguageManager::instance().getWordRegex("en");
    QRegularExpression regex(pattern);

    QString text = "I have 123 apples";
    QRegularExpressionMatchIterator it = regex.globalMatch(text);

    QStringList matches;
    while (it.hasNext()) {
        matches << it.next().captured(0);
    }

    // Should match "I", "have", "apples" but NOT "123"
    EXPECT_EQ(matches.size(), 3);
    EXPECT_FALSE(matches.contains("123"));
}

TEST_F(LanguageConfigTest, FrenchRegexRejectsPunctuation) {
    QString pattern = LanguageManager::instance().getWordRegex("fr");
    QRegularExpression regex(pattern);

    QString text = "Bonjour! Comment ça va?";
    QRegularExpressionMatchIterator it = regex.globalMatch(text);

    QStringList matches;
    while (it.hasNext()) {
        matches << it.next().captured(0);
    }

    // Should not include punctuation
    EXPECT_FALSE(matches.contains("!"));
    EXPECT_FALSE(matches.contains("?"));
    EXPECT_TRUE(matches.contains("Bonjour"));
    EXPECT_TRUE(matches.contains("Comment"));
    EXPECT_TRUE(matches.contains("ça"));
    EXPECT_TRUE(matches.contains("va"));
}

TEST_F(LanguageConfigTest, MultipleAddUpdateDelete) {
    // Add
    LanguageConfig greek("Greek", "el", "[\\x{0370}-\\x{03FF}\\x{1F00}-\\x{1FFF}]+", false, 6);
    EXPECT_TRUE(LanguageManager::instance().addCustomLanguage(greek));
    EXPECT_FALSE(LanguageManager::instance().isBuiltIn("el"));

    // Update
    LanguageConfig updatedGreek("Ελληνικά", "el", "[a-zA-Z\\x{0370}-\\x{03FF}\\x{1F00}-\\x{1FFF}]+", false, 6);
    EXPECT_TRUE(LanguageManager::instance().updateCustomLanguage("el", updatedGreek));

    // Verify update
    LanguageConfig retrieved = LanguageManager::instance().getLanguageByCode("el");
    EXPECT_EQ(retrieved.name(), "Ελληνικά");

    // Delete
    EXPECT_TRUE(LanguageManager::instance().deleteCustomLanguage("el"));

    // Verify deletion
    retrieved = LanguageManager::instance().getLanguageByCode("el");
    EXPECT_TRUE(retrieved.code().isEmpty()); // Should be empty config
}

// Character-Based Language Tests
TEST_F(LanguageConfigTest, EnglishIsNotCharacterBased) {
    EXPECT_FALSE(LanguageManager::instance().isCharacterBased("en"));
}

TEST_F(LanguageConfigTest, FrenchIsNotCharacterBased) {
    EXPECT_FALSE(LanguageManager::instance().isCharacterBased("fr"));
}

TEST_F(LanguageConfigTest, JapaneseIsCharacterBased) {
    EXPECT_TRUE(LanguageManager::instance().isCharacterBased("ja"));
}

TEST_F(LanguageConfigTest, ChineseIsCharacterBased) {
    // Chinese is now a built-in language
    EXPECT_TRUE(LanguageManager::instance().isCharacterBased("zh"));
}

TEST_F(LanguageConfigTest, CustomCharacterBasedLanguage) {
    // Add a custom character-based language (Burmese)
    LanguageConfig burmese("Burmese", "my", "[\\x{1000}-\\x{109F}]+", true, 12);
    EXPECT_TRUE(LanguageManager::instance().addCustomLanguage(burmese));

    // Verify it's marked as character-based
    EXPECT_TRUE(LanguageManager::instance().isCharacterBased("my"));

    // Clean up
    LanguageManager::instance().deleteCustomLanguage("my");
}

TEST_F(LanguageConfigTest, UnknownLanguageIsNotCharacterBased) {
    EXPECT_FALSE(LanguageManager::instance().isCharacterBased("unknown-lang"));
}

// ---------------------------------------------------------------------------
// Tokenizer integration tests
// ---------------------------------------------------------------------------

TEST_F(LanguageConfigTest, EnglishUsesRegexBackend) {
    LanguageConfig en("English", "en", "[a-zA-Z]+", false, 6);
    const Tokenizer* tok = en.tokenizer();

    ASSERT_NE(tok, nullptr);
    EXPECT_EQ(tok->backendName(), "regex");
    EXPECT_TRUE(tok->isAvailable());
}

TEST_F(LanguageConfigTest, TokenizerReturnedWithCorrectPositions) {
    LanguageConfig en("English", "en", "[a-zA-Z]+", false, 6);
    auto results = en.tokenizer()->tokenize("hello world");

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].text,     "hello");
    EXPECT_EQ(results[0].startPos, 0);
    EXPECT_EQ(results[0].endPos,   5);
    EXPECT_EQ(results[1].text,     "world");
    EXPECT_EQ(results[1].startPos, 6);
    EXPECT_EQ(results[1].endPos,  11);
}

TEST_F(LanguageConfigTest, JapaneseUsesMeCabIfAvailable) {
    LanguageConfig ja("Japanese", "ja",
                      "[\\x{3040}-\\x{309F}\\x{30A0}-\\x{30FF}\\x{4E00}-\\x{9FFF}]+",
                      true, 12);
    const Tokenizer* tok = ja.tokenizer();
    ASSERT_NE(tok, nullptr);

    bool mecabPresent = Tokenizer::createJapanese()->backendName() == "mecab";
    EXPECT_EQ(tok->backendName(), mecabPresent ? "mecab" : "regex");
}

TEST_F(LanguageConfigTest, TokenizerIsCachedBetweenCalls) {
    LanguageConfig en("English", "en", "[a-zA-Z]+", false, 6);
    const Tokenizer* first  = en.tokenizer();
    const Tokenizer* second = en.tokenizer();

    EXPECT_EQ(first, second);  // Same pointer — lazy cache hit
}

TEST_F(LanguageConfigTest, CopiedConfigGetsOwnCache) {
    LanguageConfig en("English", "en", "[a-zA-Z]+", false, 6);
    const Tokenizer* original = en.tokenizer();

    LanguageConfig copy = en;
    const Tokenizer* copyPtr = copy.tokenizer();

    // Different pointer (each copy has its own lazy cache)
    EXPECT_NE(original, copyPtr);
    // But same behaviour
    EXPECT_EQ(copyPtr->backendName(), "regex");
}

TEST_F(LanguageConfigTest, CharBasedLanguageTokenizesCorrectly) {
    // Custom char-based language using Japanese regex
    LanguageConfig charLang("Test", "xx",
                            "[\\x{3040}-\\x{309F}]+", true, 12);
    auto results = charLang.tokenizer()->tokenize("こんにちは");

    // Should be split into individual chars
    ASSERT_EQ(results.size(), 5u);
    EXPECT_EQ(results[0].startPos, 0);
    EXPECT_EQ(results[0].endPos,   1);
}
