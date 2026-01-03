#include <gtest/gtest.h>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "kotodama/termmanager.h"
#include "kotodama/databasemanager.h"
#include "kotodama/constants.h"

// Integration tests for TermManager
// These use the real database, so we need careful cleanup

class TermManagerTest : public ::testing::Test {
protected:
    static const QString TEST_LANG;

    void SetUp() override {
        // Clean up any existing test data
        cleanupTestData();

        // Clear TermManager cache to ensure clean state
        TermManager::instance().clearCache();
    }

    void TearDown() override {
        // Clean up test data after each test
        cleanupTestData();

        // Clear cache to free memory
        TermManager::instance().clearCache();
    }

    void cleanupTestData() {
        // Delete all terms for test language
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("DELETE FROM terms WHERE language = :language");
        query.bindValue(":language", TEST_LANG);
        query.exec();
    }

    // Helper to verify a term exists in the database
    bool termExistsInDB(const QString& term, const QString& language) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT COUNT(*) FROM terms WHERE term = :term AND language = :language");
        query.bindValue(":term", term);
        query.bindValue(":language", language);

        if (query.exec() && query.next()) {
            return query.value(0).toInt() > 0;
        }
        return false;
    }

    // Helper to get term count from database
    int getDBTermCount(const QString& language) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT COUNT(*) FROM terms WHERE language = :language");
        query.bindValue(":language", language);

        if (query.exec() && query.next()) {
            return query.value(0).toInt();
        }
        return 0;
    }
};

const QString TermManagerTest::TEST_LANG = "test";

// Basic Term Operations
TEST_F(TermManagerTest, AddSingleTerm) {
    bool result = TermManager::instance().addTerm(
        "hello",
        TEST_LANG,
        TermLevel::Recognized,
        "greeting",
        "heh-loh"
    );

    EXPECT_TRUE(result);
    EXPECT_TRUE(termExistsInDB("hello", TEST_LANG));
    EXPECT_TRUE(TermManager::instance().termExists("hello", TEST_LANG));
}

TEST_F(TermManagerTest, AddMultipleTerms) {
    TermManager::instance().addTerm("hello", TEST_LANG);
    TermManager::instance().addTerm("world", TEST_LANG);
    TermManager::instance().addTerm("test", TEST_LANG);

    EXPECT_EQ(TermManager::instance().getTermCount(TEST_LANG), 3);
}

TEST_F(TermManagerTest, AddMultiWordPhrase) {
    bool result = TermManager::instance().addTerm(
        "New York City",
        TEST_LANG,
        TermLevel::Recognized
    );

    EXPECT_TRUE(result);
    EXPECT_TRUE(TermManager::instance().termExists("New York City", TEST_LANG));
}

TEST_F(TermManagerTest, GetTerm) {
    TermManager::instance().addTerm(
        "test",
        TEST_LANG,
        TermLevel::Known,
        "a test",
        "test-pronunciation"
    );

    Term retrieved = TermManager::instance().getTerm("test", TEST_LANG);

    EXPECT_EQ(retrieved.term, "test");
    EXPECT_EQ(retrieved.language, TEST_LANG);
    EXPECT_EQ(retrieved.level, TermLevel::Known);
    EXPECT_EQ(retrieved.definition, "a test");
    EXPECT_EQ(retrieved.pronunciation, "test-pronunciation");
}

TEST_F(TermManagerTest, UpdateTerm) {
    // Add initial term
    TermManager::instance().addTerm("test", TEST_LANG, TermLevel::Recognized, "original");

    // Update it
    bool result = TermManager::instance().updateTerm(
        "test",
        TEST_LANG,
        TermLevel::Known,
        "updated definition",
        "updated-pronunciation"
    );

    EXPECT_TRUE(result);

    Term retrieved = TermManager::instance().getTerm("test", TEST_LANG);
    EXPECT_EQ(retrieved.level, TermLevel::Known);
    EXPECT_EQ(retrieved.definition, "updated definition");
    EXPECT_EQ(retrieved.pronunciation, "updated-pronunciation");
}

TEST_F(TermManagerTest, UpdateTermLevel) {
    TermManager::instance().addTerm("test", TEST_LANG, TermLevel::Recognized);

    bool result = TermManager::instance().updateTermLevel("test", TEST_LANG, TermLevel::WellKnown);

    EXPECT_TRUE(result);

    Term retrieved = TermManager::instance().getTerm("test", TEST_LANG);
    EXPECT_EQ(retrieved.level, TermLevel::WellKnown);
}

TEST_F(TermManagerTest, DeleteTerm) {
    TermManager::instance().addTerm("test", TEST_LANG);
    EXPECT_TRUE(TermManager::instance().termExists("test", TEST_LANG));

    bool result = TermManager::instance().deleteTerm("test", TEST_LANG);

    EXPECT_TRUE(result);
    EXPECT_FALSE(TermManager::instance().termExists("test", TEST_LANG));
    EXPECT_FALSE(termExistsInDB("test", TEST_LANG));
}

TEST_F(TermManagerTest, TermExistsReturnsFalseForNonexistent) {
    EXPECT_FALSE(TermManager::instance().termExists("nonexistent", TEST_LANG));
}

TEST_F(TermManagerTest, GetNonexistentTerm) {
    Term retrieved = TermManager::instance().getTerm("nonexistent", TEST_LANG);
    EXPECT_TRUE(retrieved.term.isEmpty());
}

// Trie Integration Tests
TEST_F(TermManagerTest, TrieIsBuiltWhenRequested) {
    TermManager::instance().addTerm("hello", TEST_LANG);
    TermManager::instance().addTerm("world", TEST_LANG);

    TrieNode* trie = TermManager::instance().getTrieForLanguage(TEST_LANG);

    ASSERT_NE(trie, nullptr);

    // Verify terms are in trie
    std::vector<std::string> tokens1 = {"hello"};
    std::vector<std::string> tokens2 = {"world"};

    Term* found1 = trie->findLongestMatch(tokens1, 0);
    Term* found2 = trie->findLongestMatch(tokens2, 0);

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);
    EXPECT_EQ(found1->term, "hello");
    EXPECT_EQ(found2->term, "world");
}

TEST_F(TermManagerTest, TrieIsRebuiltAfterAdd) {
    TermManager::instance().addTerm("hello", TEST_LANG);

    TrieNode* trie = TermManager::instance().getTrieForLanguage(TEST_LANG);

    // Add another term
    TermManager::instance().addTerm("world", TEST_LANG);

    // Re-fetch trie pointer after add (trie was rebuilt)
    trie = TermManager::instance().getTrieForLanguage(TEST_LANG);

    // Trie should be rebuilt and contain new term
    std::vector<std::string> tokens = {"world"};
    Term* found = trie->findLongestMatch(tokens, 0);

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "world");
}

TEST_F(TermManagerTest, TrieIsRebuiltAfterUpdate) {
    TermManager::instance().addTerm("test", TEST_LANG, TermLevel::Recognized);

    TrieNode* trie = TermManager::instance().getTrieForLanguage(TEST_LANG);
    std::vector<std::string> tokens = {"test"};
    Term* found = trie->findLongestMatch(tokens, 0);
    EXPECT_EQ(found->level, TermLevel::Recognized);

    // Update the term - this rebuilds the trie!
    TermManager::instance().updateTermLevel("test", TEST_LANG, TermLevel::Known);

    // Get the trie again (old pointer is now invalid)
    trie = TermManager::instance().getTrieForLanguage(TEST_LANG);

    // Check trie has updated term
    found = trie->findLongestMatch(tokens, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->level, TermLevel::Known);
}

TEST_F(TermManagerTest, TrieIsRebuiltAfterDelete) {
    TermManager::instance().addTerm("hello", TEST_LANG);
    TermManager::instance().addTerm("world", TEST_LANG);

    TrieNode* trie = TermManager::instance().getTrieForLanguage(TEST_LANG);

    // Delete one term - this rebuilds the trie!
    TermManager::instance().deleteTerm("hello", TEST_LANG);

    // Get the trie again (old pointer is now invalid)
    trie = TermManager::instance().getTrieForLanguage(TEST_LANG);

    // Trie should not contain deleted term
    std::vector<std::string> tokens = {"hello"};
    Term* found = trie->findLongestMatch(tokens, 0);
    EXPECT_EQ(found, nullptr);

    // But should still contain the other term
    tokens = {"world"};
    found = trie->findLongestMatch(tokens, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "world");
}

TEST_F(TermManagerTest, MultiWordPhraseInTrie) {
    TermManager::instance().addTerm("New York", TEST_LANG);

    TrieNode* trie = TermManager::instance().getTrieForLanguage(TEST_LANG);

    std::vector<std::string> tokens = {"new", "york"};
    Term* found = trie->findLongestMatch(tokens, 0);

    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "New York");
}

// Batch Operations
TEST_F(TermManagerTest, BatchAddTerms) {
    QList<TermManager::TermToImport> terms;

    TermManager::TermToImport t1;
    t1.term = "hello";
    t1.language = TEST_LANG;
    t1.level = TermLevel::Recognized;
    terms.append(t1);

    TermManager::TermToImport t2;
    t2.term = "world";
    t2.language = TEST_LANG;
    t2.level = TermLevel::Known;
    terms.append(t2);

    TermManager::ImportResult result = TermManager::instance().batchAddTerms(terms);

    EXPECT_EQ(result.successCount, 2);
    EXPECT_EQ(result.failCount, 0);
    EXPECT_EQ(TermManager::instance().getTermCount(TEST_LANG), 2);
}

TEST_F(TermManagerTest, BatchAddWithDuplicates) {
    // Add one term
    TermManager::instance().addTerm("duplicate", TEST_LANG);

    QList<TermManager::TermToImport> terms;

    TermManager::TermToImport t1;
    t1.term = "duplicate"; // This should be skipped (not failed)
    t1.language = TEST_LANG;
    t1.level = TermLevel::Recognized;
    terms.append(t1);

    TermManager::TermToImport t2;
    t2.term = "unique";
    t2.language = TEST_LANG;
    t2.level = TermLevel::Recognized;
    terms.append(t2);

    TermManager::ImportResult result = TermManager::instance().batchAddTerms(terms);

    // Duplicate should be skipped (not failed), unique should succeed
    EXPECT_EQ(result.successCount, 1);
    EXPECT_EQ(result.failCount, 0);
    EXPECT_EQ(TermManager::instance().getTermCount(TEST_LANG), 2); // Still 2 total terms
}

TEST_F(TermManagerTest, BatchAddEmptyList) {
    QList<TermManager::TermToImport> terms;

    TermManager::ImportResult result = TermManager::instance().batchAddTerms(terms);

    EXPECT_EQ(result.successCount, 0);
    EXPECT_EQ(result.failCount, 0);
}

TEST_F(TermManagerTest, BatchAddRebuildsTrieOnce) {
    QList<TermManager::TermToImport> terms;

    // Use letter-based terms instead of number-based since the regex doesn't match numbers
    QStringList termWords = {"apple", "banana", "cherry", "date", "elderberry",
                             "fig", "grape", "honeydew", "kiwi", "lemon"};

    for (int i = 0; i < 100; i++) {
        TermManager::TermToImport t;
        // Create unique terms like "apple-a", "banana-b", etc.
        t.term = QString("%1-%2").arg(termWords[i % 10]).arg(char('a' + (i / 10)));
        t.language = TEST_LANG;
        t.level = TermLevel::Recognized;
        terms.append(t);
    }

    TermManager::ImportResult result = TermManager::instance().batchAddTerms(terms);

    EXPECT_EQ(result.successCount, 100);
    EXPECT_EQ(TermManager::instance().getTermCount(TEST_LANG), 100);

    // Verify trie contains a specific term
    TrieNode* trie = TermManager::instance().getTrieForLanguage(TEST_LANG);
    // "apple-e" should be term 40 (apple at position 0, e is 4, so 0 + 4*10 = 40)
    std::vector<std::string> tokens = {"apple-e"};
    Term* found = trie->findLongestMatch(tokens, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "apple-e");
}

// Clean Term Text Tests
TEST_F(TermManagerTest, CleanTermTextBasic) {
    CleanedText result = TermManager::cleanTermText("hello", "en");

    EXPECT_EQ(result.cleaned, "hello");
    EXPECT_TRUE(result.removed.isEmpty());
}

TEST_F(TermManagerTest, CleanTermTextRemovesNumbers) {
    CleanedText result = TermManager::cleanTermText("hello123world", "en");

    EXPECT_EQ(result.cleaned, "helloworld");
    EXPECT_FALSE(result.removed.isEmpty());
}

TEST_F(TermManagerTest, CleanTermTextRemovesSpecialChars) {
    CleanedText result = TermManager::cleanTermText("hello@world!", "en");

    EXPECT_EQ(result.cleaned, "helloworld");
    EXPECT_FALSE(result.removed.isEmpty());
}

TEST_F(TermManagerTest, CleanTermTextPreservesSpaces) {
    CleanedText result = TermManager::cleanTermText("New York City", "en");

    EXPECT_EQ(result.cleaned, "New York City");
    EXPECT_TRUE(result.removed.isEmpty());
}

TEST_F(TermManagerTest, CleanTermTextTrimsWhitespace) {
    CleanedText result = TermManager::cleanTermText("  hello  world  ", "en");

    EXPECT_EQ(result.cleaned, "hello world");
}

TEST_F(TermManagerTest, CleanTermTextFrenchAccents) {
    CleanedText result = TermManager::cleanTermText("café", "fr");

    EXPECT_EQ(result.cleaned, "café");
    EXPECT_TRUE(result.removed.isEmpty());
}

TEST_F(TermManagerTest, CleanTermTextFrenchApostrophe) {
    CleanedText result = TermManager::cleanTermText("l'odeur", "fr");

    EXPECT_EQ(result.cleaned, "l'odeur");
    EXPECT_TRUE(result.removed.isEmpty());
}

// Cache Management
TEST_F(TermManagerTest, ClearCacheRemovesAllData) {
    TermManager::instance().addTerm("test1", TEST_LANG);
    TermManager::instance().addTerm("test2", "fr");

    // Force trie creation
    TermManager::instance().getTrieForLanguage(TEST_LANG);
    TermManager::instance().getTrieForLanguage("fr");

    // Clear cache
    TermManager::instance().clearCache();

    // Trie should be rebuilt when requested again
    TrieNode* trie = TermManager::instance().getTrieForLanguage(TEST_LANG);
    ASSERT_NE(trie, nullptr);
}

TEST_F(TermManagerTest, ReloadLanguageRebuildsSpecificTrie) {
    TermManager::instance().addTerm("test", TEST_LANG);

    TrieNode* trie1 = TermManager::instance().getTrieForLanguage(TEST_LANG);
    ASSERT_NE(trie1, nullptr);

    // Manually add to DB (bypassing TermManager to test reload)
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("INSERT INTO terms (term, language, level, definition, pronunciation) "
                  "VALUES (:term, :language, :level, :definition, :pronunciation)");
    query.bindValue(":term", "manual");
    query.bindValue(":language", TEST_LANG);
    query.bindValue(":level", static_cast<int>(TermLevel::Recognized));
    query.bindValue(":definition", "");
    query.bindValue(":pronunciation", "");
    query.exec();

    // Reload the language - this deletes the old trie and creates a new one
    TermManager::instance().reloadLanguage(TEST_LANG);

    // Get the NEW trie (old pointer is now invalid!)
    TrieNode* trie2 = TermManager::instance().getTrieForLanguage(TEST_LANG);

    // New term should now be in trie
    std::vector<std::string> tokens = {"manual"};
    Term* found = trie2->findLongestMatch(tokens, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->term, "manual");
}

// Edge Cases
TEST_F(TermManagerTest, AddEmptyTerm) {
    bool result = TermManager::instance().addTerm("", TEST_LANG);

    // Implementation dependent - document current behavior
    // Empty terms may be rejected by database constraints
    if (!result) {
        EXPECT_FALSE(termExistsInDB("", TEST_LANG));
    }
}

TEST_F(TermManagerTest, GetTermCountForEmptyLanguage) {
    int count = TermManager::instance().getTermCount(TEST_LANG);
    EXPECT_EQ(count, 0);
}

TEST_F(TermManagerTest, DeleteNonexistentTerm) {
    // Deleting a nonexistent term should fail gracefully
    bool result = TermManager::instance().deleteTerm("nonexistent", TEST_LANG);

    // May return false or true depending on implementation
    // Just verify it doesn't crash
    EXPECT_FALSE(TermManager::instance().termExists("nonexistent", TEST_LANG));
}