#ifndef BACKUPMANAGER_H
#define BACKUPMANAGER_H

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>

struct ExportResult {
    bool success;
    QString errorMessage;
    QString filePath;
    int textCount;
    int termCount;
    qint64 fileSize;

    ExportResult() : success(false), textCount(0), termCount(0), fileSize(0) {}
};

struct ImportResult {
    bool success;
    QString errorMessage;
    int textsImported;
    int termsImported;
    int textsSkipped;
    int termsUpdated;
    QStringList errors;

    ImportResult() : success(false), textsImported(0), termsImported(0),
                     textsSkipped(0), termsUpdated(0) {}
};

struct ValidationResult {
    bool valid;
    QString errorMessage;
    QString version;
    int textCount;
    int termCount;

    ValidationResult() : valid(false), textCount(0), termCount(0) {}
};

class BackupManager
{
public:
    static BackupManager& instance();

    // Export all user data to JSON file
    ExportResult exportToFile(const QString& destinationPath);

    // Import from JSON file
    ImportResult importFromFile(const QString& sourcePath, bool mergeMode = true);

    // Validation
    ValidationResult validateBackupFile(const QString& filePath);

    // Automatic backups
    ExportResult createAutomaticBackup();
    void rotateBackups(int maxBackups);
    QString getBackupDirectory() const;

private:
    BackupManager();
    ~BackupManager();
    BackupManager(const BackupManager&) = delete;
    BackupManager& operator=(const BackupManager&) = delete;

    // Export helpers
    QJsonObject createBackupJson();
    QJsonArray exportTexts();
    QJsonArray exportTerms();
    QJsonArray exportLanguageConfigs();
    QJsonArray exportUserLanguages();
    bool writeJsonToFile(const QJsonObject& json, const QString& filePath);

    // Import helpers
    bool importTexts(const QJsonArray& textsArray, bool mergeMode, ImportResult& result);
    bool importTerms(const QJsonArray& termsArray, bool mergeMode, ImportResult& result);
    bool importLanguageConfigs(const QJsonArray& configsArray);
    bool importUserLanguages(const QJsonArray& languagesArray);

    // Utilities
    QString readTextFileContent(const QString& uuid);
    bool writeTextFileContent(const QString& uuid, const QString& content);
};

#endif // BACKUPMANAGER_H
