#include <gtest/gtest.h>
#include <QStringList>
#include "kotodama/trienode.h"
#include "kotodama/term.h"

// Helper function to create test terms
Term* createTestTerm(const QString& termText, const QString& language = "en", TermLevel level = TermLevel::Recognized) {
    Term* term = new Term();
    term->id = 0;
    term->term = termText;
    term->language = language;
    term->level = level;
    term->definition = "Test definition";
    term->pronunciation = "";
    term->addedDate = "";
    term->lastModified = "";
    term->tokenCount = termText.split(' ').size();
    return term;
}

// Test Fixture for managing TrieNode lifecycle
class TrieNodeTest : public ::testing::Test {
protected:
    TrieNode* root;
    std::vector<Term*> terms; // Track terms for cleanup

    void SetUp() override {
        root = new TrieNode();
    }

    void TearDown() override {
        delete root; // This will recursively delete all children
        // Clean up terms
        for (Term* term : terms) {
            delete term;
        }
        terms.clear();
    }

    Term* addTerm(const QString& termText, const std::vector<std::string>& tokens) {
        Term* term = createTestTerm(termText);
        terms.push_back(term);
        root->insert(term, tokens);
        return term;
    }
};

// Basic Insertion Tests
TEST_F(TrieNodeTest, InsertSingleWord) {
    Term* term = addTerm("hello", {"hello"});

    Term* found = root->findLongestMatch({"hello"}, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "hello");
}

TEST_F(TrieNodeTest, InsertMultipleWords) {
    addTerm("hello", {"hello"});
    addTerm("world", {"world"});

    Term* found1 = root->findLongestMatch({"hello"}, 0);
    Term* found2 = root->findLongestMatch({"world"}, 0);

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);
    EXPECT_EQ(found1->term, "hello");
    EXPECT_EQ(found2->term, "world");
}

TEST_F(TrieNodeTest, InsertMultiWordPhrase) {
    Term* term = addTerm("New York", {"new", "york"});

    Term* found = root->findLongestMatch({"new", "york"}, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "New York");
}

// Case Insensitivity Tests
TEST_F(TrieNodeTest, CaseInsensitiveInsertion) {
    addTerm("Hello", {"Hello"});

    // Should find it with different cases
    Term* found1 = root->findLongestMatch({"hello"}, 0);
    Term* found2 = root->findLongestMatch({"HELLO"}, 0);
    Term* found3 = root->findLongestMatch({"HeLLo"}, 0);

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);
    ASSERT_NE(found3, nullptr);
    EXPECT_EQ(found1->term, "Hello");
    EXPECT_EQ(found2->term, "Hello");
    EXPECT_EQ(found3->term, "Hello");
}

TEST_F(TrieNodeTest, CaseInsensitiveMultiWord) {
    addTerm("New York City", {"new", "york", "city"});

    Term* found = root->findLongestMatch({"NEW", "YORK", "CITY"}, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "New York City");
}

// Longest Match Tests
TEST_F(TrieNodeTest, FindsLongestMatch) {
    addTerm("New", {"new"});
    Term* longerTerm = addTerm("New York", {"new", "york"});
    addTerm("New York City", {"new", "york", "city"});

    // Should find "New York" when given exactly "new york"
    Term* found = root->findLongestMatch({"new", "york"}, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "New York");
}

TEST_F(TrieNodeTest, FindsLongestMatchInContext) {
    addTerm("New", {"new"});
    addTerm("New York", {"new", "york"});
    Term* longest = addTerm("New York City", {"new", "york", "city"});

    // When given "new york city times", should find "New York City"
    std::vector<std::string> tokens = {"new", "york", "city", "times"};
    Term* found = root->findLongestMatch(tokens, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "New York City");
}

TEST_F(TrieNodeTest, FindsMatchAtDifferentStartIndex) {
    addTerm("York", {"york"});
    addTerm("York City", {"york", "city"});

    std::vector<std::string> tokens = {"new", "york", "city", "times"};

    // Start at index 1 (where "york" begins)
    Term* found = root->findLongestMatch(tokens, 1);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "York City");
}

// No Match Tests
TEST_F(TrieNodeTest, NoMatchReturnsNull) {
    addTerm("hello", {"hello"});

    Term* found = root->findLongestMatch({"world"}, 0);
    EXPECT_EQ(found, nullptr);
}

TEST_F(TrieNodeTest, PartialMatchWithoutTermReturnsNull) {
    addTerm("New York City", {"new", "york", "city"});

    // "new york" exists as a path but has no term
    Term* found = root->findLongestMatch({"new", "york"}, 0);
    EXPECT_EQ(found, nullptr);
}

TEST_F(TrieNodeTest, EmptyTrieReturnsNull) {
    Term* found = root->findLongestMatch({"hello"}, 0);
    EXPECT_EQ(found, nullptr);
}

// Edge Cases
TEST_F(TrieNodeTest, EmptyTokensReturnsNull) {
    addTerm("hello", {"hello"});

    std::vector<std::string> empty;
    Term* found = root->findLongestMatch(empty, 0);
    EXPECT_EQ(found, nullptr);
}

TEST_F(TrieNodeTest, StartIndexAtEnd) {
    addTerm("hello", {"hello"});

    std::vector<std::string> tokens = {"hello"};
    Term* found = root->findLongestMatch(tokens, 1); // Start at end
    EXPECT_EQ(found, nullptr);
}

TEST_F(TrieNodeTest, StartIndexOutOfBounds) {
    addTerm("hello", {"hello"});

    std::vector<std::string> tokens = {"hello"};
    Term* found = root->findLongestMatch(tokens, 10); // Way out of bounds
    EXPECT_EQ(found, nullptr);
}

// Overlapping Terms Tests
TEST_F(TrieNodeTest, OverlappingTerms) {
    addTerm("state", {"state"});
    addTerm("state of the art", {"state", "of", "the", "art"});

    // Should find the longer match
    std::vector<std::string> tokens = {"state", "of", "the", "art"};
    Term* found = root->findLongestMatch(tokens, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "state of the art");

    // Should find shorter match when only partial is present
    std::vector<std::string> tokens2 = {"state", "of"};
    Term* found2 = root->findLongestMatch(tokens2, 0);
    ASSERT_NE(found2, nullptr);
    EXPECT_EQ(found2->term, "state");
}

TEST_F(TrieNodeTest, PrefixMatching) {
    addTerm("run", {"run"});
    addTerm("running", {"running"});

    // Should find exact matches, not prefixes
    Term* found1 = root->findLongestMatch({"run"}, 0);
    Term* found2 = root->findLongestMatch({"running"}, 0);

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);
    EXPECT_EQ(found1->term, "run");
    EXPECT_EQ(found2->term, "running");
}

// French Language Tests
TEST_F(TrieNodeTest, FrenchWordWithApostrophe) {
    Term* term = createTestTerm("l'odeur", "fr");
    terms.push_back(term);
    root->insert(term, {"l'odeur"});

    Term* found = root->findLongestMatch({"l'odeur"}, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "l'odeur");
}

TEST_F(TrieNodeTest, FrenchMultiWordPhrase) {
    Term* term = createTestTerm("à la mode", "fr");
    terms.push_back(term);
    root->insert(term, {"à", "la", "mode"});

    Term* found = root->findLongestMatch({"à", "la", "mode"}, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "à la mode");
}

// Complex Scenario Tests
TEST_F(TrieNodeTest, MultiplePhrasesWithCommonPrefix) {
    addTerm("United", {"united"});
    addTerm("United States", {"united", "states"});
    addTerm("United States of America", {"united", "states", "of", "america"});

    std::vector<std::string> tokens1 = {"united"};
    std::vector<std::string> tokens2 = {"united", "states"};
    std::vector<std::string> tokens3 = {"united", "states", "of", "america"};

    Term* found1 = root->findLongestMatch(tokens1, 0);
    Term* found2 = root->findLongestMatch(tokens2, 0);
    Term* found3 = root->findLongestMatch(tokens3, 0);

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);
    ASSERT_NE(found3, nullptr);
    EXPECT_EQ(found1->term, "United");
    EXPECT_EQ(found2->term, "United States");
    EXPECT_EQ(found3->term, "United States of America");
}

TEST_F(TrieNodeTest, MatchInMiddleOfSentence) {
    addTerm("once upon a time", {"once", "upon", "a", "time"});

    std::vector<std::string> tokens = {"long", "long", "ago", "once", "upon", "a", "time", "there", "lived"};

    // Find it starting at index 3
    Term* found = root->findLongestMatch(tokens, 3);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "once upon a time");

    // Should not find it at index 0
    Term* notFound = root->findLongestMatch(tokens, 0);
    EXPECT_EQ(notFound, nullptr);
}

// Unicode Case Handling Tests (Bug Fix Verification)
TEST_F(TrieNodeTest, UnicodeUppercaseFrenchAccents) {
    // Insert lowercase French words
    addTerm("café", {"café"});
    addTerm("résumé", {"résumé"});
    addTerm("naïve", {"naïve"});

    // Search with UPPERCASE versions - should match due to proper Unicode lowercasing
    Term* found1 = root->findLongestMatch({"CAFÉ"}, 0);
    Term* found2 = root->findLongestMatch({"RÉSUMÉ"}, 0);
    Term* found3 = root->findLongestMatch({"NAÏVE"}, 0);

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);
    ASSERT_NE(found3, nullptr);
    EXPECT_EQ(found1->term, "café");
    EXPECT_EQ(found2->term, "résumé");
    EXPECT_EQ(found3->term, "naïve");
}

TEST_F(TrieNodeTest, UnicodeUppercaseFrenchApostrophe) {
    // Insert with lowercase
    addTerm("l'odeur", {"l'odeur"});
    addTerm("d'été", {"d'été"});

    // Search with uppercase É
    Term* found1 = root->findLongestMatch({"L'ODEUR"}, 0);
    Term* found2 = root->findLongestMatch({"D'ÉTÉ"}, 0);

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);
    EXPECT_EQ(found1->term, "l'odeur");
    EXPECT_EQ(found2->term, "d'été");
}

TEST_F(TrieNodeTest, UnicodeMixedCaseFrenchPhrase) {
    addTerm("à la mode", {"à", "la", "mode"});

    // Search with mixed case including uppercase À
    Term* found = root->findLongestMatch({"À", "La", "MODE"}, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "à la mode");
}

TEST_F(TrieNodeTest, UnicodeInsertUppercaseSearchLowercase) {
    // Insert with UPPERCASE accented characters
    Term* term1 = createTestTerm("CAFÉ");
    terms.push_back(term1);
    root->insert(term1, {"CAFÉ"});

    Term* term2 = createTestTerm("RÉSUMÉ");
    terms.push_back(term2);
    root->insert(term2, {"RÉSUMÉ"});

    // Search with lowercase - should still match
    Term* found1 = root->findLongestMatch({"café"}, 0);
    Term* found2 = root->findLongestMatch({"résumé"}, 0);

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);
    EXPECT_EQ(found1->term, "CAFÉ");
    EXPECT_EQ(found2->term, "RÉSUMÉ");
}

TEST_F(TrieNodeTest, UnicodeGermanUmlaut) {
    addTerm("über", {"über"});
    addTerm("schön", {"schön"});

    // Test with uppercase umlauts
    Term* found1 = root->findLongestMatch({"ÜBER"}, 0);
    Term* found2 = root->findLongestMatch({"SCHÖN"}, 0);

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);
    EXPECT_EQ(found1->term, "über");
    EXPECT_EQ(found2->term, "schön");
}

TEST_F(TrieNodeTest, UnicodeSpanishAccents) {
    addTerm("señor", {"señor"});
    addTerm("mañana", {"mañana"});

    // Test with uppercase Ñ
    Term* found1 = root->findLongestMatch({"SEÑOR"}, 0);
    Term* found2 = root->findLongestMatch({"MAÑANA"}, 0);

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);
    EXPECT_EQ(found1->term, "señor");
    EXPECT_EQ(found2->term, "mañana");
}

TEST_F(TrieNodeTest, UnicodeCyrillicCharacters) {
    addTerm("привет", {"привет"}); // Russian "hello"

    // Test with uppercase Cyrillic
    Term* found = root->findLongestMatch({"ПРИВЕТ"}, 0);

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "привет");
}
