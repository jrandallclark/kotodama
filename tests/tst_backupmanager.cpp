#include <gtest/gtest.h>
#include "kotodama/backupmanager.h"
#include "kotodama/databasemanager.h"
#include "kotodama/librarymanager.h"
#include "kotodama/term.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class BackupManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        QStandardPaths::setTestModeEnabled(true);
        DatabaseManager::instance().initialize();
        LibraryManager::instance().initialize();

        tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                  + "/kotodama-backup-test";
        QDir().mkpath(tempDir);
    }

    void TearDown() override {
        // Clean up temp directory
        QDir(tempDir).removeRecursively();

        // Clean up test database
        QString libraryPath = LibraryManager::instance().getLibraryPath();
        QDir(libraryPath).removeRecursively();
    }

    QString createTestTextFile(const QString& uuid, const QString& content) {
        QString filePath = LibraryManager::instance().getTextFilePath(uuid);
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return QString();
        }
        QTextStream out(&file);
        out << content;
        file.close();
        return filePath;
    }

    QString tempDir;
};

TEST_F(BackupManagerTest, ExportEmptyDatabase) {
    QString backupPath = tempDir + "/empty-backup.kotodama.json";

    ExportResult result = BackupManager::instance().exportToFile(backupPath);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.textCount, 0);
    EXPECT_EQ(result.termCount, 0);
    EXPECT_GT(result.fileSize, 0);  // File exists with JSON structure

    // Verify file was created
    QFile file(backupPath);
    EXPECT_TRUE(file.exists());
}

TEST_F(BackupManagerTest, ExportSingleText) {
    // Add a test text to database
    QString uuid = "test-uuid-123";
    QString content = "Hello, World!";
    createTestTextFile(uuid, content);

    DatabaseManager::instance().addText(
        uuid,
        "Test Title",
        "test.txt",
        "en",
        1000,
        13
    );

    // Export
    QString backupPath = tempDir + "/single-text-backup.kotodama.json";
    ExportResult result = BackupManager::instance().exportToFile(backupPath);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.textCount, 1);
    EXPECT_EQ(result.termCount, 0);

    // Verify JSON contains the text with embedded content
    QFile file(backupPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    ASSERT_TRUE(doc.isObject());
    QJsonObject root = doc.object();
    QJsonArray texts = root["database"].toObject()["texts"].toArray();

    ASSERT_EQ(texts.size(), 1);
    QJsonObject textObj = texts[0].toObject();
    EXPECT_EQ(textObj["uuid"].toString(), uuid);
    EXPECT_EQ(textObj["title"].toString(), "Test Title");
    EXPECT_EQ(textObj["content"].toString(), content);
}

TEST_F(BackupManagerTest, ExportMultipleTexts) {
    // Add multiple texts
    for (int i = 0; i < 3; ++i) {
        QString uuid = QString("uuid-%1").arg(i);
        QString content = QString("Content %1").arg(i);
        createTestTextFile(uuid, content);

        DatabaseManager::instance().addText(
            uuid,
            QString("Title %1").arg(i),
            QString("file%1.txt").arg(i),
            "en",
            1000,
            content.length()
        );
    }

    // Export
    QString backupPath = tempDir + "/multiple-texts-backup.kotodama.json";
    ExportResult result = BackupManager::instance().exportToFile(backupPath);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.textCount, 3);
}

TEST_F(BackupManagerTest, ExportWithTerms) {
    // Add test terms
    DatabaseManager::instance().addTerm("hello", "en", TermLevel::Known, "greeting", "");
    DatabaseManager::instance().addTerm("world", "en", TermLevel::Learning, "planet", "");

    // Export
    QString backupPath = tempDir + "/terms-backup.kotodama.json";
    ExportResult result = BackupManager::instance().exportToFile(backupPath);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.termCount, 2);

    // Verify JSON contains terms
    QFile file(backupPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonArray terms = doc.object()["database"].toObject()["terms"].toArray();
    ASSERT_EQ(terms.size(), 2);
}

TEST_F(BackupManagerTest, ExportInvalidPath) {
    QString invalidPath = "/nonexistent/directory/backup.kotodama.json";

    ExportResult result = BackupManager::instance().exportToFile(invalidPath);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.isEmpty());
}

TEST_F(BackupManagerTest, ValidateValidFile) {
    // Create a valid backup first
    DatabaseManager::instance().addTerm("test", "en", TermLevel::Recognized, "", "");

    QString backupPath = tempDir + "/valid-backup.kotodama.json";
    ExportResult exportResult = BackupManager::instance().exportToFile(backupPath);
    ASSERT_TRUE(exportResult.success);

    // Validate it
    ValidationResult result = BackupManager::instance().validateBackupFile(backupPath);

    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.errorMessage.isEmpty());
    EXPECT_EQ(result.termCount, 1);
}

TEST_F(BackupManagerTest, ValidateInvalidJson) {
    // Create a file with invalid JSON
    QString invalidPath = tempDir + "/invalid.kotodama.json";
    QFile file(invalidPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("{ this is not valid JSON }");
    file.close();

    ValidationResult result = BackupManager::instance().validateBackupFile(invalidPath);

    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.errorMessage.isEmpty());
}

TEST_F(BackupManagerTest, ValidateMissingFile) {
    QString nonexistentPath = tempDir + "/does-not-exist.kotodama.json";

    ValidationResult result = BackupManager::instance().validateBackupFile(nonexistentPath);

    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.errorMessage, "File does not exist");
}

TEST_F(BackupManagerTest, ValidateUnsupportedVersion) {
    // Create a backup with unsupported version
    QString backupPath = tempDir + "/unsupported-version.kotodama.json";
    QJsonObject root;
    root["version"] = "2.0";  // Unsupported version
    root["database"] = QJsonObject();

    QFile file(backupPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    QJsonDocument doc(root);
    file.write(doc.toJson());
    file.close();

    ValidationResult result = BackupManager::instance().validateBackupFile(backupPath);

    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.errorMessage.contains("Unsupported version"));
}

TEST_F(BackupManagerTest, ImportValidBackup) {
    // Create a backup
    QString uuid = "import-test-uuid";
    createTestTextFile(uuid, "Import test content");
    DatabaseManager::instance().addText(uuid, "Import Test", "import.txt", "en", 1000, 18);
    DatabaseManager::instance().addTerm("imported", "en", TermLevel::Known, "brought in", "");

    QString backupPath = tempDir + "/import-test.kotodama.json";
    ExportResult exportResult = BackupManager::instance().exportToFile(backupPath);
    ASSERT_TRUE(exportResult.success);

    // Clear database
    DatabaseManager::instance().deleteText(uuid);
    DatabaseManager::instance().deleteTerm("imported", "en");

    // Import
    ImportResult result = BackupManager::instance().importFromFile(backupPath, true);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.textsImported, 1);
    EXPECT_EQ(result.termsImported, 1);

    // Verify data was restored
    TextInfo text = DatabaseManager::instance().getText(uuid);
    EXPECT_EQ(text.uuid, uuid);
    EXPECT_EQ(text.title, "Import Test");

    Term term = DatabaseManager::instance().getTerm("imported", "en");
    EXPECT_EQ(term.term, "imported");
    EXPECT_EQ(term.level, TermLevel::Known);
}

TEST_F(BackupManagerTest, ImportInvalidJson) {
    QString invalidPath = tempDir + "/invalid-import.kotodama.json";
    QFile file(invalidPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("not json");
    file.close();

    ImportResult result = BackupManager::instance().importFromFile(invalidPath, true);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.isEmpty());
}

TEST_F(BackupManagerTest, ImportMergeDuplicateTexts) {
    // Add a text to database
    QString uuid = "duplicate-uuid";
    createTestTextFile(uuid, "Original content");
    DatabaseManager::instance().addText(uuid, "Original Title", "original.txt", "en", 1000, 16);

    // Create backup with same UUID but different content
    QString backupPath = tempDir + "/duplicate-test.kotodama.json";
    QJsonObject root;
    root["version"] = "1.0";
    root["exportDate"] = "2024-01-01T00:00:00";

    QJsonObject database;
    QJsonArray texts;
    QJsonObject textObj;
    textObj["uuid"] = uuid;
    textObj["title"] = "Backup Title";
    textObj["originalFilename"] = "backup.txt";
    textObj["language"] = "en";
    textObj["fileSize"] = 2000;
    textObj["characterCount"] = 14;
    textObj["content"] = "Backup content";
    textObj["readingPosition"] = 0;
    textObj["currentPage"] = 1;
    textObj["totalPages"] = 1;
    textObj["addedDate"] = "";
    textObj["lastOpened"] = "";
    texts.append(textObj);
    database["texts"] = texts;
    database["terms"] = QJsonArray();
    database["languageConfigs"] = QJsonArray();
    database["userLanguages"] = QJsonArray();
    root["database"] = database;

    QFile file(backupPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    QJsonDocument doc(root);
    file.write(doc.toJson());
    file.close();

    // Import in merge mode
    ImportResult result = BackupManager::instance().importFromFile(backupPath, true);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.textsSkipped, 1);  // Should skip duplicate
    EXPECT_EQ(result.textsImported, 0);

    // Verify original data is preserved
    TextInfo text = DatabaseManager::instance().getText(uuid);
    EXPECT_EQ(text.title, "Original Title");
}

TEST_F(BackupManagerTest, ImportUpdatesExistingTerms) {
    // Add a term
    DatabaseManager::instance().addTerm("update", "en", TermLevel::Recognized, "old definition", "");

    // Create backup with updated term
    QString backupPath = tempDir + "/update-term-test.kotodama.json";
    QJsonObject root;
    root["version"] = "1.0";
    root["exportDate"] = "2024-01-01T00:00:00";

    QJsonObject database;
    QJsonArray terms;
    QJsonObject termObj;
    termObj["term"] = "update";
    termObj["language"] = "en";
    termObj["level"] = static_cast<int>(TermLevel::Known);
    termObj["definition"] = "new definition";
    termObj["pronunciation"] = "updated";
    termObj["addedDate"] = "";
    termObj["lastModified"] = "";
    termObj["tokenCount"] = 1;
    terms.append(termObj);
    database["terms"] = terms;
    database["texts"] = QJsonArray();
    database["languageConfigs"] = QJsonArray();
    database["userLanguages"] = QJsonArray();
    root["database"] = database;

    QFile file(backupPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    QJsonDocument doc(root);
    file.write(doc.toJson());
    file.close();

    // Import in merge mode
    ImportResult result = BackupManager::instance().importFromFile(backupPath, true);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.termsUpdated, 1);
    EXPECT_EQ(result.termsImported, 0);

    // Verify term was updated
    Term term = DatabaseManager::instance().getTerm("update", "en");
    EXPECT_EQ(term.level, TermLevel::Known);
    EXPECT_EQ(term.definition, "new definition");
    EXPECT_EQ(term.pronunciation, "updated");
}

TEST_F(BackupManagerTest, RoundTripExportImport) {
    // Create test data
    QString uuid1 = "round-trip-1";
    QString uuid2 = "round-trip-2";
    createTestTextFile(uuid1, "Content 1");
    createTestTextFile(uuid2, "Content 2");

    DatabaseManager::instance().addText(uuid1, "Title 1", "file1.txt", "en", 1000, 9);
    DatabaseManager::instance().addText(uuid2, "Title 2", "file2.txt", "fr", 2000, 9);
    DatabaseManager::instance().addTerm("roundtrip", "en", TermLevel::WellKnown, "test", "rt");
    DatabaseManager::instance().addTerm("voyage", "fr", TermLevel::Learning, "trip", "vwayazh");

    // Export
    QString backupPath = tempDir + "/roundtrip.kotodama.json";
    ExportResult exportResult = BackupManager::instance().exportToFile(backupPath);
    ASSERT_TRUE(exportResult.success);
    EXPECT_EQ(exportResult.textCount, 2);
    EXPECT_EQ(exportResult.termCount, 2);

    // Clear all data
    DatabaseManager::instance().deleteText(uuid1);
    DatabaseManager::instance().deleteText(uuid2);
    DatabaseManager::instance().deleteTerm("roundtrip", "en");
    DatabaseManager::instance().deleteTerm("voyage", "fr");

    // Verify data is gone
    EXPECT_TRUE(DatabaseManager::instance().getText(uuid1).uuid.isEmpty());
    EXPECT_TRUE(DatabaseManager::instance().getTerm("roundtrip", "en").term.isEmpty());

    // Import
    ImportResult importResult = BackupManager::instance().importFromFile(backupPath, true);
    ASSERT_TRUE(importResult.success);
    EXPECT_EQ(importResult.textsImported, 2);
    EXPECT_EQ(importResult.termsImported, 2);

    // Verify all data restored correctly
    TextInfo text1 = DatabaseManager::instance().getText(uuid1);
    EXPECT_EQ(text1.uuid, uuid1);
    EXPECT_EQ(text1.title, "Title 1");
    EXPECT_EQ(text1.language, "en");

    TextInfo text2 = DatabaseManager::instance().getText(uuid2);
    EXPECT_EQ(text2.uuid, uuid2);
    EXPECT_EQ(text2.title, "Title 2");
    EXPECT_EQ(text2.language, "fr");

    Term term1 = DatabaseManager::instance().getTerm("roundtrip", "en");
    EXPECT_EQ(term1.term, "roundtrip");
    EXPECT_EQ(term1.level, TermLevel::WellKnown);
    EXPECT_EQ(term1.definition, "test");

    Term term2 = DatabaseManager::instance().getTerm("voyage", "fr");
    EXPECT_EQ(term2.term, "voyage");
    EXPECT_EQ(term2.level, TermLevel::Learning);

    // Verify text file content was restored
    QString filePath1 = LibraryManager::instance().getTextFilePath(uuid1);
    QFile file1(filePath1);
    ASSERT_TRUE(file1.open(QIODevice::ReadOnly));
    QString content1 = file1.readAll();
    file1.close();
    EXPECT_EQ(content1, "Content 1");
}
