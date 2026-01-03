#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "kotodama/databasemanager.h"
#include "kotodama/term.h"

#include <QStandardPaths>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDateTime>
#include <QThread>

class DatabaseManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Enable test mode for isolated paths
        QStandardPaths::setTestModeEnabled(true);

        // Initialize fresh database
        DatabaseManager::instance().initialize();

        // Clean up all data from previous tests
        QSqlQuery query(DatabaseManager::instance().database());
        query.exec("DELETE FROM terms");
        query.exec("DELETE FROM texts");
    }

    void TearDown() override {
        // Clean up all test data
        QSqlQuery query(DatabaseManager::instance().database());
        query.exec("DELETE FROM terms");
        query.exec("DELETE FROM texts");
    }

    // Helper to count rows in a table
    int countRows(const QString& table) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare(QString("SELECT COUNT(*) FROM %1").arg(table));
        if (query.exec() && query.next()) {
            return query.value(0).toInt();
        }
        return -1;
    }
};

// Initialization Tests
TEST_F(DatabaseManagerTest, InitializeCreatesDatabase) {
    QString libraryPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString dbPath = libraryPath + "/database.sqlite";
    EXPECT_TRUE(QFile::exists(dbPath));
}

TEST_F(DatabaseManagerTest, InitializeCreatesTables) {
    // Check that tables exist by querying them
    QSqlQuery query(DatabaseManager::instance().database());

    EXPECT_TRUE(query.exec("SELECT COUNT(*) FROM texts"));
    EXPECT_TRUE(query.exec("SELECT COUNT(*) FROM terms"));
    EXPECT_TRUE(query.exec("SELECT COUNT(*) FROM language_configs"));
}

TEST_F(DatabaseManagerTest, InitializeTwiceSucceeds) {
    bool result1 = DatabaseManager::instance().initialize();
    bool result2 = DatabaseManager::instance().initialize();

    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);
}

// Text Management Tests
TEST_F(DatabaseManagerTest, AddTextReturnsUuid) {
    QString uuid = DatabaseManager::instance().addText(
        "test-uuid", "Test Title", "test.txt", "en", 1000, 500
    );

    EXPECT_EQ(uuid, "test-uuid");
}

TEST_F(DatabaseManagerTest, AddTextInsertsIntoDatabase) {
    DatabaseManager::instance().addText(
        "test-uuid", "Test Title", "test.txt", "en", 1000, 500
    );

    EXPECT_EQ(countRows("texts"), 1);
}

TEST_F(DatabaseManagerTest, AddMultipleTexts) {
    DatabaseManager::instance().addText("uuid1", "Title 1", "file1.txt", "en", 100, 50);
    DatabaseManager::instance().addText("uuid2", "Title 2", "file2.txt", "fr", 200, 100);
    DatabaseManager::instance().addText("uuid3", "Title 3", "file3.txt", "en", 300, 150);

    EXPECT_EQ(countRows("texts"), 3);
}

TEST_F(DatabaseManagerTest, GetTextReturnsCorrectData) {
    DatabaseManager::instance().addText(
        "test-uuid", "My Title", "original.txt", "en", 1234, 567
    );

    TextInfo info = DatabaseManager::instance().getText("test-uuid");

    EXPECT_EQ(info.uuid, "test-uuid");
    EXPECT_EQ(info.title, "My Title");
    EXPECT_EQ(info.originalFilename, "original.txt");
    EXPECT_EQ(info.language, "en");
    EXPECT_EQ(info.fileSize, 1234);
    EXPECT_EQ(info.characterCount, 567);
    EXPECT_FALSE(info.addedDate.isEmpty());
}

TEST_F(DatabaseManagerTest, GetNonexistentTextReturnsEmpty) {
    TextInfo info = DatabaseManager::instance().getText("nonexistent-uuid");

    EXPECT_TRUE(info.uuid.isEmpty());
}

TEST_F(DatabaseManagerTest, GetTextsReturnsAllTexts) {
    DatabaseManager::instance().addText("uuid1", "Title 1", "file1.txt", "en", 100, 50);
    DatabaseManager::instance().addText("uuid2", "Title 2", "file2.txt", "fr", 200, 100);
    DatabaseManager::instance().addText("uuid3", "Title 3", "file3.txt", "en", 300, 150);

    QList<TextInfo> texts = DatabaseManager::instance().getTexts();

    EXPECT_EQ(texts.size(), 3);
}

TEST_F(DatabaseManagerTest, GetTextsFiltersByLanguage) {
    DatabaseManager::instance().addText("uuid1", "English 1", "file1.txt", "en", 100, 50);
    DatabaseManager::instance().addText("uuid2", "French 1", "file2.txt", "fr", 200, 100);
    DatabaseManager::instance().addText("uuid3", "English 2", "file3.txt", "en", 300, 150);

    QList<TextInfo> enTexts = DatabaseManager::instance().getTexts("en");
    QList<TextInfo> frTexts = DatabaseManager::instance().getTexts("fr");

    EXPECT_EQ(enTexts.size(), 2);
    EXPECT_EQ(frTexts.size(), 1);
    EXPECT_EQ(frTexts[0].title, "French 1");
}

TEST_F(DatabaseManagerTest, GetTextsReturnsEmptyListWhenNoTexts) {
    QList<TextInfo> texts = DatabaseManager::instance().getTexts();

    EXPECT_EQ(texts.size(), 0);
}

TEST_F(DatabaseManagerTest, DeleteTextRemovesFromDatabase) {
    DatabaseManager::instance().addText("test-uuid", "Title", "file.txt", "en", 100, 50);

    bool result = DatabaseManager::instance().deleteText("test-uuid");

    EXPECT_TRUE(result);
    EXPECT_EQ(countRows("texts"), 0);
}

TEST_F(DatabaseManagerTest, DeleteNonexistentTextSucceeds) {
    // SQLite DELETE succeeds even if no rows are affected
    bool result = DatabaseManager::instance().deleteText("nonexistent-uuid");

    EXPECT_TRUE(result);
}

TEST_F(DatabaseManagerTest, UpdateReadingPosition) {
    DatabaseManager::instance().addText("test-uuid", "Title", "file.txt", "en", 100, 50);

    DatabaseManager::instance().updateReadingPosition("test-uuid", 42);

    TextInfo info = DatabaseManager::instance().getText("test-uuid");
    EXPECT_EQ(info.readingPosition, 42);
}

TEST_F(DatabaseManagerTest, UpdatePageInfo) {
    DatabaseManager::instance().addText("test-uuid", "Title", "file.txt", "en", 100, 50);

    DatabaseManager::instance().updatePageInfo("test-uuid", 5, 10);

    TextInfo info = DatabaseManager::instance().getText("test-uuid");
    EXPECT_EQ(info.currentPage, 5);
    EXPECT_EQ(info.totalPages, 10);
}

TEST_F(DatabaseManagerTest, UpdateLastOpened) {
    DatabaseManager::instance().addText("test-uuid", "Title", "file.txt", "en", 100, 50);

    TextInfo infoBefore = DatabaseManager::instance().getText("test-uuid");

    // Wait a bit to ensure timestamp difference
    QThread::msleep(10);

    DatabaseManager::instance().updateLastOpened("test-uuid");

    TextInfo infoAfter = DatabaseManager::instance().getText("test-uuid");
    EXPECT_FALSE(infoAfter.lastOpened.isEmpty());
}

TEST_F(DatabaseManagerTest, GetTextFilePathConstructsCorrectPath) {
    QString path = DatabaseManager::instance().getTextFilePath("test-uuid");

    EXPECT_TRUE(path.contains("test-uuid"));
    EXPECT_TRUE(path.endsWith(".txt"));
    EXPECT_TRUE(path.contains("/texts/"));
}

TEST_F(DatabaseManagerTest, GetTextFilePathIsValid) {
    QString path = DatabaseManager::instance().getTextFilePath("test-uuid");

    EXPECT_FALSE(path.isEmpty());
    EXPECT_TRUE(path.contains("test-uuid"));
}

// Term Management Tests
TEST_F(DatabaseManagerTest, AddTermSucceeds) {
    bool result = DatabaseManager::instance().addTerm(
        "hello", "en", TermLevel::Recognized, "greeting", "heh-loh"
    );

    EXPECT_TRUE(result);
    EXPECT_EQ(countRows("terms"), 1);
}

TEST_F(DatabaseManagerTest, AddMultipleTerms) {
    DatabaseManager::instance().addTerm("hello", "en", TermLevel::Recognized);
    DatabaseManager::instance().addTerm("world", "en", TermLevel::Learning);
    DatabaseManager::instance().addTerm("bonjour", "fr", TermLevel::Known);

    EXPECT_EQ(countRows("terms"), 3);
}

TEST_F(DatabaseManagerTest, AddTermWithAllFields) {
    DatabaseManager::instance().addTerm(
        "café", "fr", TermLevel::Learning, "coffee shop", "ka-fay"
    );

    Term term = DatabaseManager::instance().getTerm("café", "fr");

    EXPECT_EQ(term.term, "café");
    EXPECT_EQ(term.language, "fr");
    EXPECT_EQ(term.level, TermLevel::Learning);
    EXPECT_EQ(term.definition, "coffee shop");
    EXPECT_EQ(term.pronunciation, "ka-fay");
    EXPECT_EQ(term.tokenCount, 1);
    EXPECT_FALSE(term.addedDate.isEmpty());
    EXPECT_FALSE(term.lastModified.isEmpty());
}

TEST_F(DatabaseManagerTest, AddDuplicateTermFails) {
    bool result1 = DatabaseManager::instance().addTerm("hello", "en", TermLevel::Recognized);
    bool result2 = DatabaseManager::instance().addTerm("hello", "en", TermLevel::Known);

    EXPECT_TRUE(result1);
    EXPECT_FALSE(result2);  // Duplicate should fail
    EXPECT_EQ(countRows("terms"), 1);
}

TEST_F(DatabaseManagerTest, AddSameTermDifferentLanguageSucceeds) {
    bool result1 = DatabaseManager::instance().addTerm("hello", "en", TermLevel::Recognized);
    bool result2 = DatabaseManager::instance().addTerm("hello", "fr", TermLevel::Known);

    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);
    EXPECT_EQ(countRows("terms"), 2);
}

TEST_F(DatabaseManagerTest, GetTermReturnsCorrectData) {
    DatabaseManager::instance().addTerm(
        "test", "en", TermLevel::Learning, "a trial", "test"
    );

    Term term = DatabaseManager::instance().getTerm("test", "en");

    EXPECT_EQ(term.term, "test");
    EXPECT_EQ(term.language, "en");
    EXPECT_EQ(term.level, TermLevel::Learning);
    EXPECT_EQ(term.definition, "a trial");
    EXPECT_EQ(term.pronunciation, "test");
}

TEST_F(DatabaseManagerTest, GetNonexistentTermReturnsEmpty) {
    Term term = DatabaseManager::instance().getTerm("nonexistent", "en");

    EXPECT_TRUE(term.term.isEmpty());
    EXPECT_EQ(term.id, 0);
}

TEST_F(DatabaseManagerTest, GetTermsReturnsAllTerms) {
    DatabaseManager::instance().addTerm("hello", "en", TermLevel::Recognized);
    DatabaseManager::instance().addTerm("world", "en", TermLevel::Learning);
    DatabaseManager::instance().addTerm("bonjour", "fr", TermLevel::Known);

    QList<Term> terms = DatabaseManager::instance().getTerms();

    EXPECT_EQ(terms.size(), 3);
}

TEST_F(DatabaseManagerTest, GetTermsFiltersByLanguage) {
    DatabaseManager::instance().addTerm("hello", "en", TermLevel::Recognized);
    DatabaseManager::instance().addTerm("world", "en", TermLevel::Learning);
    DatabaseManager::instance().addTerm("bonjour", "fr", TermLevel::Known);

    QList<Term> enTerms = DatabaseManager::instance().getTerms("en");
    QList<Term> frTerms = DatabaseManager::instance().getTerms("fr");

    EXPECT_EQ(enTerms.size(), 2);
    EXPECT_EQ(frTerms.size(), 1);
    EXPECT_EQ(frTerms[0].term, "bonjour");
}

TEST_F(DatabaseManagerTest, GetTermsByLevelFiltersCorrectly) {
    DatabaseManager::instance().addTerm("new1", "en", TermLevel::Recognized);
    DatabaseManager::instance().addTerm("new2", "en", TermLevel::Recognized);
    DatabaseManager::instance().addTerm("learning", "en", TermLevel::Learning);
    DatabaseManager::instance().addTerm("known", "en", TermLevel::Known);

    QList<Term> newTerms = DatabaseManager::instance().getTermsByLevel("en", TermLevel::Recognized);
    QList<Term> learningTerms = DatabaseManager::instance().getTermsByLevel("en", TermLevel::Learning);
    QList<Term> knownTerms = DatabaseManager::instance().getTermsByLevel("en", TermLevel::Known);

    EXPECT_EQ(newTerms.size(), 2);
    EXPECT_EQ(learningTerms.size(), 1);
    EXPECT_EQ(knownTerms.size(), 1);
}

TEST_F(DatabaseManagerTest, UpdateTermChangesAllFields) {
    DatabaseManager::instance().addTerm("hello", "en", TermLevel::Recognized, "old def", "old-pron");

    bool result = DatabaseManager::instance().updateTerm(
        "hello", "en", TermLevel::Known, "new definition", "new-pronunciation"
    );

    EXPECT_TRUE(result);

    Term term = DatabaseManager::instance().getTerm("hello", "en");
    EXPECT_EQ(term.level, TermLevel::Known);
    EXPECT_EQ(term.definition, "new definition");
    EXPECT_EQ(term.pronunciation, "new-pronunciation");
}

TEST_F(DatabaseManagerTest, UpdateNonexistentTermSucceeds) {
    // SQLite UPDATE succeeds even if no rows are affected
    bool result = DatabaseManager::instance().updateTerm(
        "nonexistent", "en", TermLevel::Known, "def", "pron"
    );

    EXPECT_TRUE(result);
}

TEST_F(DatabaseManagerTest, UpdateTermLevelChangesOnlyLevel) {
    DatabaseManager::instance().addTerm("hello", "en", TermLevel::Recognized, "greeting", "heh-loh");

    DatabaseManager::instance().updateTermLevel("hello", "en", TermLevel::WellKnown);

    Term term = DatabaseManager::instance().getTerm("hello", "en");
    EXPECT_EQ(term.level, TermLevel::WellKnown);
    EXPECT_EQ(term.definition, "greeting");  // Unchanged
    EXPECT_EQ(term.pronunciation, "heh-loh");  // Unchanged
}

TEST_F(DatabaseManagerTest, DeleteTermRemovesFromDatabase) {
    DatabaseManager::instance().addTerm("hello", "en", TermLevel::Recognized);

    bool result = DatabaseManager::instance().deleteTerm("hello", "en");

    EXPECT_TRUE(result);
    EXPECT_EQ(countRows("terms"), 0);
}

TEST_F(DatabaseManagerTest, DeleteTermOnlyDeletesSpecificLanguage) {
    DatabaseManager::instance().addTerm("hello", "en", TermLevel::Recognized);
    DatabaseManager::instance().addTerm("hello", "fr", TermLevel::Known);

    DatabaseManager::instance().deleteTerm("hello", "en");

    EXPECT_EQ(countRows("terms"), 1);

    Term remaining = DatabaseManager::instance().getTerm("hello", "fr");
    EXPECT_EQ(remaining.language, "fr");
}

TEST_F(DatabaseManagerTest, DeleteNonexistentTermSucceeds) {
    bool result = DatabaseManager::instance().deleteTerm("nonexistent", "en");

    EXPECT_TRUE(result);
}

TEST_F(DatabaseManagerTest, GetTermCountReturnsCorrectCount) {
    DatabaseManager::instance().addTerm("hello", "en", TermLevel::Recognized);
    DatabaseManager::instance().addTerm("world", "en", TermLevel::Learning);
    DatabaseManager::instance().addTerm("bonjour", "fr", TermLevel::Known);

    EXPECT_EQ(DatabaseManager::instance().getTermCount("en"), 2);
    EXPECT_EQ(DatabaseManager::instance().getTermCount("fr"), 1);
}

TEST_F(DatabaseManagerTest, GetTermCountForEmptyLanguageReturnsZero) {
    EXPECT_EQ(DatabaseManager::instance().getTermCount("nonexistent"), 0);
}

TEST_F(DatabaseManagerTest, GetTermCountByLevelReturnsCorrectCount) {
    DatabaseManager::instance().addTerm("new1", "en", TermLevel::Recognized);
    DatabaseManager::instance().addTerm("new2", "en", TermLevel::Recognized);
    DatabaseManager::instance().addTerm("learning", "en", TermLevel::Learning);

    EXPECT_EQ(DatabaseManager::instance().getTermCountByLevel("en", TermLevel::Recognized), 2);
    EXPECT_EQ(DatabaseManager::instance().getTermCountByLevel("en", TermLevel::Learning), 1);
    EXPECT_EQ(DatabaseManager::instance().getTermCountByLevel("en", TermLevel::Known), 0);
}

TEST_F(DatabaseManagerTest, TokenCountStoredCorrectlyForSingleWord) {
    DatabaseManager::instance().addTerm("hello", "en", TermLevel::Recognized);

    Term term = DatabaseManager::instance().getTerm("hello", "en");
    EXPECT_EQ(term.tokenCount, 1);
}

TEST_F(DatabaseManagerTest, TokenCountStoredCorrectlyForMultiWordPhrase) {
    DatabaseManager::instance().addTerm("hello world", "en", TermLevel::Recognized);

    Term term = DatabaseManager::instance().getTerm("hello world", "en");
    EXPECT_EQ(term.tokenCount, 2);
}

TEST_F(DatabaseManagerTest, TokenCountHandlesFrenchApostrophe) {
    // "l'école" should be 1 token in French (includes apostrophe in word)
    DatabaseManager::instance().addTerm("l'école", "fr", TermLevel::Recognized);

    Term term = DatabaseManager::instance().getTerm("l'école", "fr");
    EXPECT_EQ(term.tokenCount, 1);
}

// Integration Tests
TEST_F(DatabaseManagerTest, CompleteTextLifecycle) {
    // Add
    QString uuid = DatabaseManager::instance().addText(
        "test-uuid", "Test Title", "test.txt", "en", 1000, 500
    );
    EXPECT_FALSE(uuid.isEmpty());

    // Read
    TextInfo info = DatabaseManager::instance().getText(uuid);
    EXPECT_EQ(info.title, "Test Title");
    EXPECT_EQ(info.readingPosition, 0);

    // Update
    DatabaseManager::instance().updateReadingPosition(uuid, 100);
    DatabaseManager::instance().updatePageInfo(uuid, 3, 10);

    info = DatabaseManager::instance().getText(uuid);
    EXPECT_EQ(info.readingPosition, 100);
    EXPECT_EQ(info.currentPage, 3);
    EXPECT_EQ(info.totalPages, 10);

    // Delete
    DatabaseManager::instance().deleteText(uuid);
    info = DatabaseManager::instance().getText(uuid);
    EXPECT_TRUE(info.uuid.isEmpty());
}

TEST_F(DatabaseManagerTest, CompleteTermLifecycle) {
    // Add
    bool added = DatabaseManager::instance().addTerm(
        "hello", "en", TermLevel::Recognized, "greeting", "heh-loh"
    );
    EXPECT_TRUE(added);

    // Read
    Term term = DatabaseManager::instance().getTerm("hello", "en");
    EXPECT_EQ(term.term, "hello");
    EXPECT_EQ(term.level, TermLevel::Recognized);

    // Update level
    DatabaseManager::instance().updateTermLevel("hello", "en", TermLevel::Learning);
    term = DatabaseManager::instance().getTerm("hello", "en");
    EXPECT_EQ(term.level, TermLevel::Learning);

    // Update all fields
    DatabaseManager::instance().updateTerm(
        "hello", "en", TermLevel::Known, "a greeting", "HELL-oh"
    );
    term = DatabaseManager::instance().getTerm("hello", "en");
    EXPECT_EQ(term.level, TermLevel::Known);
    EXPECT_EQ(term.definition, "a greeting");
    EXPECT_EQ(term.pronunciation, "HELL-oh");

    // Delete
    DatabaseManager::instance().deleteTerm("hello", "en");
    term = DatabaseManager::instance().getTerm("hello", "en");
    EXPECT_TRUE(term.term.isEmpty());
}

TEST_F(DatabaseManagerTest, MixedTextsAndTerms) {
    // Add both texts and terms
    DatabaseManager::instance().addText("uuid1", "Title 1", "file1.txt", "en", 100, 50);
    DatabaseManager::instance().addText("uuid2", "Title 2", "file2.txt", "fr", 200, 100);

    DatabaseManager::instance().addTerm("hello", "en", TermLevel::Recognized);
    DatabaseManager::instance().addTerm("world", "en", TermLevel::Learning);
    DatabaseManager::instance().addTerm("bonjour", "fr", TermLevel::Known);

    // Verify independence
    EXPECT_EQ(DatabaseManager::instance().getTexts("en").size(), 1);
    EXPECT_EQ(DatabaseManager::instance().getTexts("fr").size(), 1);
    EXPECT_EQ(DatabaseManager::instance().getTerms("en").size(), 2);
    EXPECT_EQ(DatabaseManager::instance().getTerms("fr").size(), 1);

    // Delete text shouldn't affect terms
    DatabaseManager::instance().deleteText("uuid1");
    EXPECT_EQ(DatabaseManager::instance().getTerms("en").size(), 2);

    // Delete term shouldn't affect texts
    DatabaseManager::instance().deleteTerm("bonjour", "fr");
    EXPECT_EQ(DatabaseManager::instance().getTexts("fr").size(), 1);
}
