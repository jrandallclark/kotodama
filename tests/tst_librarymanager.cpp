#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "kotodama/librarymanager.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QStandardPaths>
#include <QFileInfo>

class LibraryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Enable test mode for isolated paths
        QStandardPaths::setTestModeEnabled(true);

        // Initialize LibraryManager
        LibraryManager::instance().initialize();

        // Create temp directory for test files
        tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/kotodama-library-test";
        QDir dir;
        dir.mkpath(tempDir);
    }

    void TearDown() override {
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

    // Helper: Read content from a file
    QString readFile(const QString& filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString();
        }

        QTextStream in(&file);
        QString content = in.readAll();
        file.close();

        return content;
    }

    QString tempDir;
};

// Initialization Tests
TEST_F(LibraryManagerTest, InitializeCreatesDirectories) {
    // Clean up first
    QString libraryPath = LibraryManager::instance().getLibraryPath();
    QDir dir(libraryPath);
    if (dir.exists()) {
        dir.removeRecursively();
    }

    bool result = LibraryManager::instance().initialize();

    EXPECT_TRUE(result);
    EXPECT_TRUE(QDir(libraryPath).exists());
    EXPECT_TRUE(QDir(LibraryManager::instance().getTextsPath()).exists());
}

TEST_F(LibraryManagerTest, InitializeSucceedsWhenDirectoriesExist) {
    // Initialize twice
    bool result1 = LibraryManager::instance().initialize();
    bool result2 = LibraryManager::instance().initialize();

    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);
}

TEST_F(LibraryManagerTest, GetLibraryPathReturnsNonEmpty) {
    QString path = LibraryManager::instance().getLibraryPath();

    EXPECT_FALSE(path.isEmpty());
}

TEST_F(LibraryManagerTest, GetTextsPathIncludesLibraryPath) {
    QString libraryPath = LibraryManager::instance().getLibraryPath();
    QString textsPath = LibraryManager::instance().getTextsPath();

    EXPECT_TRUE(textsPath.startsWith(libraryPath));
    EXPECT_TRUE(textsPath.endsWith("/texts"));
}

// Import Tests
TEST_F(LibraryManagerTest, ImportValidFileSucceeds) {
    QString testFile = createTestFile("test.txt", "Hello, World!");

    QString uuid = LibraryManager::instance().importText(testFile);

    EXPECT_FALSE(uuid.isEmpty());
}

TEST_F(LibraryManagerTest, ImportReturnsValidUuid) {
    QString testFile = createTestFile("test.txt", "Content");

    QString uuid = LibraryManager::instance().importText(testFile);

    // UUID should be 36 characters with hyphens or 32 without
    EXPECT_TRUE(uuid.length() == 32 || uuid.length() == 36);
}

TEST_F(LibraryManagerTest, ImportCreatesDifferentUuidsForMultipleFiles) {
    QString file1 = createTestFile("test1.txt", "Content 1");
    QString file2 = createTestFile("test2.txt", "Content 2");

    QString uuid1 = LibraryManager::instance().importText(file1);
    QString uuid2 = LibraryManager::instance().importText(file2);

    EXPECT_NE(uuid1, uuid2);
}

TEST_F(LibraryManagerTest, ImportSameFileTwiceCreatesDifferentUuids) {
    QString testFile = createTestFile("test.txt", "Same content");

    QString uuid1 = LibraryManager::instance().importText(testFile);
    QString uuid2 = LibraryManager::instance().importText(testFile);

    EXPECT_NE(uuid1, uuid2);
}

TEST_F(LibraryManagerTest, ImportNonexistentFileFails) {
    QString uuid = LibraryManager::instance().importText("/nonexistent/file.txt");

    EXPECT_TRUE(uuid.isEmpty());
}

TEST_F(LibraryManagerTest, ImportCreatesFileInCorrectLocation) {
    QString testFile = createTestFile("test.txt", "Test content");

    QString uuid = LibraryManager::instance().importText(testFile);
    QString importedPath = LibraryManager::instance().getTextFilePath(uuid);

    EXPECT_TRUE(QFile::exists(importedPath));
}

TEST_F(LibraryManagerTest, ImportedFileHasCorrectContent) {
    QString originalContent = "This is test content\nwith multiple lines.";
    QString testFile = createTestFile("test.txt", originalContent);

    QString uuid = LibraryManager::instance().importText(testFile);
    QString importedPath = LibraryManager::instance().getTextFilePath(uuid);
    QString importedContent = readFile(importedPath);

    EXPECT_EQ(originalContent, importedContent);
}

TEST_F(LibraryManagerTest, ImportHandlesUnicodeContent) {
    QString unicodeContent = "Français: L'école, À bientôt!\nРусский: Привет мир!";
    QString testFile = createTestFile("unicode.txt", unicodeContent);

    QString uuid = LibraryManager::instance().importText(testFile);
    QString importedPath = LibraryManager::instance().getTextFilePath(uuid);
    QString importedContent = readFile(importedPath);

    EXPECT_EQ(unicodeContent, importedContent);
}

TEST_F(LibraryManagerTest, ImportEmptyFile) {
    QString testFile = createTestFile("empty.txt", "");

    QString uuid = LibraryManager::instance().importText(testFile);

    EXPECT_FALSE(uuid.isEmpty());

    QString importedPath = LibraryManager::instance().getTextFilePath(uuid);
    EXPECT_TRUE(QFile::exists(importedPath));
}

// Delete Tests
TEST_F(LibraryManagerTest, DeleteExistingFileSucceeds) {
    QString testFile = createTestFile("test.txt", "Content to delete");
    QString uuid = LibraryManager::instance().importText(testFile);

    bool result = LibraryManager::instance().deleteText(uuid);

    EXPECT_TRUE(result);
}

TEST_F(LibraryManagerTest, DeleteActuallyRemovesFile) {
    QString testFile = createTestFile("test.txt", "Content to delete");
    QString uuid = LibraryManager::instance().importText(testFile);
    QString importedPath = LibraryManager::instance().getTextFilePath(uuid);

    LibraryManager::instance().deleteText(uuid);

    EXPECT_FALSE(QFile::exists(importedPath));
}

TEST_F(LibraryManagerTest, DeleteNonexistentFileFails) {
    bool result = LibraryManager::instance().deleteText("nonexistent-uuid");

    EXPECT_FALSE(result);
}

TEST_F(LibraryManagerTest, DeleteTwiceFails) {
    QString testFile = createTestFile("test.txt", "Content");
    QString uuid = LibraryManager::instance().importText(testFile);

    bool firstDelete = LibraryManager::instance().deleteText(uuid);
    bool secondDelete = LibraryManager::instance().deleteText(uuid);

    EXPECT_TRUE(firstDelete);
    EXPECT_FALSE(secondDelete);
}

// File Info Tests
TEST_F(LibraryManagerTest, GetFileSizeReturnsCorrectSize) {
    QString content = "12345";  // 5 bytes
    QString testFile = createTestFile("test.txt", content);

    qint64 size = LibraryManager::instance().getFileSize(testFile);

    EXPECT_EQ(size, 5);
}

TEST_F(LibraryManagerTest, GetFileSizeForLargerFile) {
    QString content(1000, 'A');  // 1000 characters
    QString testFile = createTestFile("large.txt", content);

    qint64 size = LibraryManager::instance().getFileSize(testFile);

    EXPECT_EQ(size, 1000);
}

TEST_F(LibraryManagerTest, GetFileSizeForEmptyFile) {
    QString testFile = createTestFile("empty.txt", "");

    qint64 size = LibraryManager::instance().getFileSize(testFile);

    EXPECT_EQ(size, 0);
}

TEST_F(LibraryManagerTest, GetFileSizeForNonexistentFile) {
    qint64 size = LibraryManager::instance().getFileSize("/nonexistent/file.txt");

    EXPECT_EQ(size, 0);
}

TEST_F(LibraryManagerTest, GetCharacterCountReturnsCorrectCount) {
    QString content = "Hello";  // 5 characters
    QString testFile = createTestFile("test.txt", content);

    int count = LibraryManager::instance().getCharacterCount(testFile);

    EXPECT_EQ(count, 5);
}

TEST_F(LibraryManagerTest, GetCharacterCountHandlesMultipleLines) {
    QString content = "Line 1\nLine 2\nLine 3";  // 20 characters including newlines
    QString testFile = createTestFile("multiline.txt", content);

    int count = LibraryManager::instance().getCharacterCount(testFile);

    EXPECT_EQ(count, 20);
}

TEST_F(LibraryManagerTest, GetCharacterCountHandlesUnicode) {
    QString content = "café";  // 4 characters (é is one character)
    QString testFile = createTestFile("unicode.txt", content);

    int count = LibraryManager::instance().getCharacterCount(testFile);

    EXPECT_EQ(count, 4);
}

TEST_F(LibraryManagerTest, GetCharacterCountForEmptyFile) {
    QString testFile = createTestFile("empty.txt", "");

    int count = LibraryManager::instance().getCharacterCount(testFile);

    EXPECT_EQ(count, 0);
}

TEST_F(LibraryManagerTest, GetCharacterCountForNonexistentFile) {
    int count = LibraryManager::instance().getCharacterCount("/nonexistent/file.txt");

    EXPECT_EQ(count, 0);
}

// Path Tests
TEST_F(LibraryManagerTest, GetTextFilePathConstructsCorrectPath) {
    QString uuid = "test-uuid-1234";

    QString path = LibraryManager::instance().getTextFilePath(uuid);

    EXPECT_TRUE(path.contains(uuid));
    EXPECT_TRUE(path.endsWith(".txt"));
}

TEST_F(LibraryManagerTest, GetTextFilePathIncludesTextsDirectory) {
    QString uuid = "test-uuid";
    QString textsPath = LibraryManager::instance().getTextsPath();

    QString path = LibraryManager::instance().getTextFilePath(uuid);

    EXPECT_TRUE(path.startsWith(textsPath));
}

// Integration Tests
TEST_F(LibraryManagerTest, CompleteImportAndDeleteCycle) {
    QString originalContent = "Test content for import and delete";
    QString testFile = createTestFile("cycle.txt", originalContent);

    // Import
    QString uuid = LibraryManager::instance().importText(testFile);
    ASSERT_FALSE(uuid.isEmpty());

    // Verify imported
    QString importedPath = LibraryManager::instance().getTextFilePath(uuid);
    ASSERT_TRUE(QFile::exists(importedPath));

    QString importedContent = readFile(importedPath);
    EXPECT_EQ(originalContent, importedContent);

    // Get file info
    qint64 size = LibraryManager::instance().getFileSize(importedPath);
    int charCount = LibraryManager::instance().getCharacterCount(importedPath);
    EXPECT_GT(size, 0);
    EXPECT_EQ(charCount, originalContent.length());

    // Delete
    bool deleted = LibraryManager::instance().deleteText(uuid);
    EXPECT_TRUE(deleted);
    EXPECT_FALSE(QFile::exists(importedPath));
}

TEST_F(LibraryManagerTest, MultipleImportsAndDeletes) {
    QStringList uuids;

    // Import multiple files
    for (int i = 0; i < 5; i++) {
        QString content = QString("Content %1").arg(i);
        QString testFile = createTestFile(QString("test%1.txt").arg(i), content);
        QString uuid = LibraryManager::instance().importText(testFile);
        ASSERT_FALSE(uuid.isEmpty());
        uuids.append(uuid);
    }

    // Verify all exist
    for (const QString& uuid : uuids) {
        QString path = LibraryManager::instance().getTextFilePath(uuid);
        EXPECT_TRUE(QFile::exists(path));
    }

    // Delete all
    for (const QString& uuid : uuids) {
        bool deleted = LibraryManager::instance().deleteText(uuid);
        EXPECT_TRUE(deleted);
    }

    // Verify all deleted
    for (const QString& uuid : uuids) {
        QString path = LibraryManager::instance().getTextFilePath(uuid);
        EXPECT_FALSE(QFile::exists(path));
    }
}
