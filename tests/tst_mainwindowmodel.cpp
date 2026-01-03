#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "kotodama/mainwindowmodel.h"
#include "kotodama/databasemanager.h"
#include "kotodama/librarymanager.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QStandardPaths>

class MainWindowModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Enable test mode for isolated paths
        QStandardPaths::setTestModeEnabled(true);

        // Initialize managers
        DatabaseManager::instance().initialize();
        LibraryManager::instance().initialize();

        // Create temp directory for test files
        tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/kotodama-mainwindowmodel-test";
        QDir dir;
        dir.mkpath(tempDir);

        // Create model instance
        model = new MainWindowModel();
    }

    void TearDown() override {
        delete model;

        // Clean up imported files
        QString textsPath = LibraryManager::instance().getTextsPath();
        QDir textsDir(textsPath);
        if (textsDir.exists()) {
            textsDir.removeRecursively();
        }

        // Clean up temp test files
        QDir dir(tempDir);
        if (dir.exists()) {
            dir.removeRecursively();
        }
    }

    // Helper: Create a test file with given content
    QString createTestFile(const QString& filename, const QString& content) {
        QString filePath = tempDir + "/" + filename;
        QFile file(filePath);

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return QString();
        }

        QTextStream out(&file);
        out << content;
        file.close();

        return filePath;
    }

    MainWindowModel* model;
    QString tempDir;
};

// Test importing an empty file
TEST_F(MainWindowModelTest, ImportEmptyFile) {
    QString filePath = createTestFile("empty.txt", "");

    ImportTextResult result = model->importText(filePath, "Empty Test", "English");

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.isEmpty());
    EXPECT_TRUE(result.errorMessage.contains("empty") || result.errorMessage.contains("no text"));
}

// Test importing a file with only whitespace (spaces)
TEST_F(MainWindowModelTest, ImportWhitespaceOnlyFile_Spaces) {
    QString filePath = createTestFile("whitespace_spaces.txt", "     ");

    ImportTextResult result = model->importText(filePath, "Whitespace Test", "English");

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.isEmpty());
    EXPECT_TRUE(result.errorMessage.contains("whitespace") || result.errorMessage.contains("no text"));
}

// Test importing a file with only whitespace (tabs and newlines)
TEST_F(MainWindowModelTest, ImportWhitespaceOnlyFile_Mixed) {
    QString filePath = createTestFile("whitespace_mixed.txt", "\t\n  \n\t  \n");

    ImportTextResult result = model->importText(filePath, "Whitespace Test", "English");

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.isEmpty());
    EXPECT_TRUE(result.errorMessage.contains("whitespace") || result.errorMessage.contains("no text"));
}

// Test importing a valid file with actual content
TEST_F(MainWindowModelTest, ImportValidFile) {
    QString filePath = createTestFile("valid.txt", "This is valid content for testing.");

    ImportTextResult result = model->importText(filePath, "Valid Test", "English");

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.errorMessage.isEmpty());
    EXPECT_FALSE(result.uuid.isEmpty());
}

// Test importing a file with leading/trailing whitespace but valid content
TEST_F(MainWindowModelTest, ImportFileWithWhitespaceAndContent) {
    QString filePath = createTestFile("content_with_whitespace.txt", "\n\n  Valid content here  \n\n");

    ImportTextResult result = model->importText(filePath, "Content with Whitespace", "English");

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.errorMessage.isEmpty());
    EXPECT_FALSE(result.uuid.isEmpty());
}

// Test that empty file validation happens before database insertion
TEST_F(MainWindowModelTest, EmptyFileDoesNotCreateDatabaseEntry) {
    // Get initial count
    QList<TextDisplayItem> initialTexts = model->getTexts();
    int initialCount = initialTexts.size();

    QString filePath = createTestFile("empty_db_test.txt", "");

    ImportTextResult result = model->importText(filePath, "Empty DB Test", "English");

    EXPECT_FALSE(result.success);

    // Verify no new entry was created in database
    QList<TextDisplayItem> finalTexts = model->getTexts();
    EXPECT_EQ(finalTexts.size(), initialCount);
}
