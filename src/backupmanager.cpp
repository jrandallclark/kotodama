#include "kotodama/backupmanager.h"
#include "kotodama/databasemanager.h"
#include "kotodama/librarymanager.h"
#include "kotodama/term.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QSettings>
#include <QSet>
#include <QDebug>

BackupManager::BackupManager()
{
}

BackupManager::~BackupManager()
{
}

BackupManager& BackupManager::instance()
{
    static BackupManager instance;
    return instance;
}

// ============================================================================
// EXPORT FUNCTIONALITY
// ============================================================================

ExportResult BackupManager::exportToFile(const QString& destinationPath)
{
    ExportResult result;
    result.success = false;
    result.filePath = destinationPath;

    // Step 1: Create JSON structure
    QJsonObject backup = createBackupJson();
    if (backup.isEmpty()) {
        result.errorMessage = "Failed to create backup data";
        return result;
    }

    // Step 2: Write to file
    if (!writeJsonToFile(backup, destinationPath)) {
        result.errorMessage = "Failed to write backup file";
        return result;
    }

    // Step 3: Populate success metrics
    QFileInfo fileInfo(destinationPath);
    result.fileSize = fileInfo.size();

    QJsonObject database = backup["database"].toObject();
    result.textCount = database["texts"].toArray().size();
    result.termCount = database["terms"].toArray().size();
    result.success = true;

    qDebug() << "Export successful:" << result.textCount << "texts,"
             << result.termCount << "terms," << result.fileSize << "bytes";

    return result;
}

QJsonObject BackupManager::createBackupJson()
{
    QJsonObject root;

    // Metadata
    root["version"] = "1.0";
    root["exportDate"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["appVersion"] = "0.1";

    // Database content
    QJsonObject database;
    database["texts"] = exportTexts();
    database["terms"] = exportTerms();
    database["languageConfigs"] = exportLanguageConfigs();
    database["userLanguages"] = exportUserLanguages();

    root["database"] = database;

    return root;
}

QJsonArray BackupManager::exportTexts()
{
    QJsonArray textsArray;

    // Get all texts from database (no language filter = all)
    QList<TextInfo> texts = DatabaseManager::instance().getTexts();

    for (const TextInfo& text : texts) {
        QJsonObject textObj;
        textObj["uuid"] = text.uuid;
        textObj["title"] = text.title;
        textObj["originalFilename"] = text.originalFilename;
        textObj["language"] = text.language;
        textObj["fileSize"] = static_cast<qint64>(text.fileSize);
        textObj["characterCount"] = text.characterCount;
        textObj["addedDate"] = text.addedDate;
        textObj["lastOpened"] = text.lastOpened;
        textObj["readingPosition"] = text.readingPosition;
        textObj["currentPage"] = text.currentPage;
        textObj["totalPages"] = text.totalPages;

        // Read and embed text file content
        QString content = readTextFileContent(text.uuid);
        if (!content.isNull()) {  // null = error, empty string is valid
            textObj["content"] = content;
            textsArray.append(textObj);
        } else {
            qDebug() << "Warning: Failed to read text file:" << text.uuid;
            // Continue with other texts, don't fail entire export
        }
    }

    qDebug() << "Exported" << textsArray.size() << "texts";
    return textsArray;
}

QString BackupManager::readTextFileContent(const QString& uuid)
{
    QString filePath = LibraryManager::instance().getTextFilePath(uuid);
    QFile file(filePath);

    if (!file.exists()) {
        qDebug() << "Text file does not exist:" << filePath;
        return QString();  // null QString = error
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Cannot open text file:" << filePath;
        return QString();
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    return content;  // Empty string is valid, null QString is error
}

QJsonArray BackupManager::exportTerms()
{
    QJsonArray termsArray;

    // Get all terms (no language filter = all)
    QList<Term> terms = DatabaseManager::instance().getTerms();

    for (const Term& term : terms) {
        QJsonObject termObj;
        termObj["term"] = term.term;
        termObj["language"] = term.language;
        termObj["level"] = static_cast<int>(term.level);
        termObj["definition"] = term.definition;
        termObj["pronunciation"] = term.pronunciation;
        termObj["addedDate"] = term.addedDate;
        termObj["lastModified"] = term.lastModified;
        termObj["tokenCount"] = term.tokenCount;

        termsArray.append(termObj);
    }

    qDebug() << "Exported" << termsArray.size() << "terms";
    return termsArray;
}

QJsonArray BackupManager::exportLanguageConfigs()
{
    QJsonArray configsArray;

    // Get all language configs from database
    QList<LanguageConfig> configs = DatabaseManager::instance().getLanguageConfigs();

    for (const LanguageConfig& config : configs) {
        QJsonObject configObj;
        configObj["code"] = config.code();
        configObj["name"] = config.name();
        configObj["wordRegex"] = config.wordRegex();
        configObj["isCharBased"] = config.isCharBased();
        configObj["tokenLimit"] = config.tokenLimit();

        configsArray.append(configObj);
    }

    qDebug() << "Exported" << configsArray.size() << "language configs";
    return configsArray;
}

QJsonArray BackupManager::exportUserLanguages()
{
    QJsonArray languagesArray;

    // Get user's learning languages
    QStringList languages = DatabaseManager::instance().getUserLanguages();

    for (const QString& lang : languages) {
        languagesArray.append(lang);
    }

    qDebug() << "Exported" << languagesArray.size() << "user languages";
    return languagesArray;
}

bool BackupManager::writeJsonToFile(const QJsonObject& json, const QString& filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Cannot open file for writing:" << filePath;
        return false;
    }

    QJsonDocument doc(json);
    QByteArray jsonData = doc.toJson(QJsonDocument::Indented);

    qint64 bytesWritten = file.write(jsonData);
    file.close();

    if (bytesWritten == -1 || bytesWritten != jsonData.size()) {
        qDebug() << "Failed to write complete data to file";
        return false;
    }

    return true;
}

// ============================================================================
// IMPORT FUNCTIONALITY
// ============================================================================

ValidationResult BackupManager::validateBackupFile(const QString& filePath)
{
    ValidationResult result;
    result.valid = false;

    // Check file exists
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        result.errorMessage = "File does not exist";
        return result;
    }

    // Check readable
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = "Cannot read file (permission denied?)";
        return result;
    }

    // Parse JSON
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        result.errorMessage = "Invalid JSON format";
        return result;
    }

    QJsonObject root = doc.object();

    // Check required fields
    if (!root.contains("version")) {
        result.errorMessage = "Missing version field";
        return result;
    }

    if (!root.contains("database")) {
        result.errorMessage = "Missing database field";
        return result;
    }

    // Check version compatibility
    QString version = root["version"].toString();
    if (version != "1.0") {
        result.errorMessage = "Unsupported version: " + version;
        return result;
    }

    // Extract metadata
    QJsonObject db = root["database"].toObject();
    result.version = root["exportDate"].toString();
    result.textCount = db["texts"].toArray().size();
    result.termCount = db["terms"].toArray().size();
    result.valid = true;

    return result;
}

ImportResult BackupManager::importFromFile(const QString& sourcePath, bool mergeMode)
{
    ImportResult result;
    result.success = false;

    // Step 1: Read JSON file
    QFile file(sourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = "Cannot open backup file";
        return result;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    // Step 2: Parse JSON
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject()) {
        result.errorMessage = "Invalid JSON format";
        return result;
    }

    QJsonObject root = doc.object();

    // Step 3: Validate version
    if (!root.contains("version") || !root.contains("database")) {
        result.errorMessage = "Invalid backup file structure";
        return result;
    }

    QString version = root["version"].toString();
    if (version != "1.0") {
        result.errorMessage = "Unsupported backup version: " + version;
        return result;
    }

    QJsonObject database = root["database"].toObject();

    // Step 4: Import in dependency order
    // Language configs first (needed for terms/texts)
    if (database.contains("languageConfigs")) {
        if (!importLanguageConfigs(database["languageConfigs"].toArray())) {
            result.errors.append("Some language configs failed to import");
        }
    }

    // User languages second
    if (database.contains("userLanguages")) {
        if (!importUserLanguages(database["userLanguages"].toArray())) {
            result.errors.append("Some user languages failed to import");
        }
    }

    // Texts third (creates files + DB records)
    if (database.contains("texts")) {
        if (!importTexts(database["texts"].toArray(), mergeMode, result)) {
            result.errorMessage = "Failed to import texts";
            return result;
        }
    }

    // Terms last (references texts)
    if (database.contains("terms")) {
        if (!importTerms(database["terms"].toArray(), mergeMode, result)) {
            result.errorMessage = "Failed to import terms";
            return result;
        }
    }

    result.success = true;
    qDebug() << "Import completed:" << result.textsImported << "texts imported,"
             << result.textsSkipped << "texts skipped," << result.termsImported
             << "terms imported," << result.termsUpdated << "terms updated";

    return result;
}

bool BackupManager::importTexts(const QJsonArray& textsArray, bool mergeMode, ImportResult& result)
{
    for (const QJsonValue& value : textsArray) {
        if (!value.isObject()) {
            result.errors.append("Invalid text entry in backup");
            continue;
        }

        QJsonObject textObj = value.toObject();
        QString uuid = textObj["uuid"].toString();

        // Check if text already exists
        TextInfo existing = DatabaseManager::instance().getText(uuid);
        if (!existing.uuid.isEmpty()) {
            if (mergeMode) {
                result.textsSkipped++;
                continue;  // Skip existing
            }
            // Replace mode: delete old first
            DatabaseManager::instance().deleteText(uuid);
            LibraryManager::instance().deleteText(uuid);
        }

        // Write text file
        QString content = textObj["content"].toString();
        if (!writeTextFileContent(uuid, content)) {
            result.errors.append("Failed to write text file: " + uuid);
            continue;
        }

        // Add to database
        QString addedUuid = DatabaseManager::instance().addText(
            uuid,
            textObj["title"].toString(),
            textObj["originalFilename"].toString(),
            textObj["language"].toString(),
            static_cast<qint64>(textObj["fileSize"].toDouble()),
            textObj["characterCount"].toInt()
        );

        if (addedUuid.isEmpty()) {
            result.errors.append("Failed to add text to database: " + uuid);
            // Clean up file
            LibraryManager::instance().deleteText(uuid);
            continue;
        }

        // Restore reading progress
        DatabaseManager::instance().updateReadingPosition(
            uuid,
            textObj["readingPosition"].toInt()
        );
        DatabaseManager::instance().updatePageInfo(
            uuid,
            textObj["currentPage"].toInt(),
            textObj["totalPages"].toInt()
        );

        result.textsImported++;
    }

    return true;  // Partial success is ok
}

bool BackupManager::writeTextFileContent(const QString& uuid, const QString& content)
{
    // Ensure texts directory exists
    LibraryManager::instance().initialize();

    QString filePath = LibraryManager::instance().getTextFilePath(uuid);
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Cannot create text file:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out << content;
    file.close();

    return true;
}

bool BackupManager::importTerms(const QJsonArray& termsArray, bool mergeMode, ImportResult& result)
{
    for (const QJsonValue& value : termsArray) {
        if (!value.isObject()) {
            result.errors.append("Invalid term entry in backup");
            continue;
        }

        QJsonObject termObj = value.toObject();
        QString term = termObj["term"].toString();
        QString language = termObj["language"].toString();

        // Check if term exists
        Term existing = DatabaseManager::instance().getTerm(term, language);

        if (!existing.term.isEmpty()) {
            if (mergeMode) {
                // Update existing term with imported data
                bool updated = DatabaseManager::instance().updateTerm(
                    term,
                    language,
                    static_cast<TermLevel>(termObj["level"].toInt()),
                    termObj["definition"].toString(),
                    termObj["pronunciation"].toString()
                );

                if (updated) {
                    result.termsUpdated++;
                } else {
                    result.errors.append("Failed to update term: " + term);
                }
            }
            // In replace mode, existing terms stay (we don't delete individual terms)
            continue;
        }

        // Add new term
        bool added = DatabaseManager::instance().addTerm(
            term,
            language,
            static_cast<TermLevel>(termObj["level"].toInt()),
            termObj["definition"].toString(),
            termObj["pronunciation"].toString()
        );

        if (added) {
            result.termsImported++;
        } else {
            result.errors.append("Failed to import term: " + term);
        }
    }

    return true;  // Partial success is ok
}

bool BackupManager::importLanguageConfigs(const QJsonArray& configsArray)
{
    // Get existing configs to check for duplicates
    QList<LanguageConfig> existingConfigs = DatabaseManager::instance().getLanguageConfigs();
    QSet<QString> existingCodes;
    for (const LanguageConfig& config : existingConfigs) {
        existingCodes.insert(config.code());
    }

    for (const QJsonValue& value : configsArray) {
        if (!value.isObject()) {
            qDebug() << "Invalid language config entry in backup";
            continue;
        }

        QJsonObject configObj = value.toObject();
        QString code = configObj["code"].toString();

        // Check if config already exists
        if (existingCodes.contains(code)) {
            // Skip existing configs (preserve user's customizations)
            continue;
        }

        // Add new language config
        LanguageConfig config(
            configObj["name"].toString(),
            code,
            configObj["wordRegex"].toString(),
            configObj["isCharBased"].toBool(),
            configObj["tokenLimit"].toInt()
        );

        bool added = DatabaseManager::instance().addLanguageConfig(config);
        if (!added) {
            qDebug() << "Failed to import language config:" << code;
        }
    }

    return true;  // Partial success is ok
}

bool BackupManager::importUserLanguages(const QJsonArray& languagesArray)
{
    for (const QJsonValue& value : languagesArray) {
        if (!value.isString()) {
            qDebug() << "Invalid user language entry in backup";
            continue;
        }

        QString languageCode = value.toString();

        // Check if already in user's learning languages
        QStringList existing = DatabaseManager::instance().getUserLanguages();
        if (existing.contains(languageCode)) {
            continue;  // Skip existing
        }

        // Add to user languages
        bool added = DatabaseManager::instance().addUserLanguage(languageCode);
        if (!added) {
            qDebug() << "Failed to import user language:" << languageCode;
        }
    }

    return true;  // Partial success is ok
}

// ============================================================================
// AUTOMATIC BACKUP FUNCTIONALITY
// ============================================================================

QString BackupManager::getBackupDirectory() const
{
    QString libraryPath = LibraryManager::instance().getLibraryPath();
    return libraryPath + "/backups";
}

ExportResult BackupManager::createAutomaticBackup()
{
    ExportResult result;
    result.success = false;

    // Ensure backup directory exists
    QString backupDir = getBackupDirectory();
    QDir dir;
    if (!dir.exists(backupDir)) {
        if (!dir.mkpath(backupDir)) {
            result.errorMessage = "Failed to create backup directory";
            qDebug() << "Failed to create backup directory:" << backupDir;
            return result;
        }
    }

    // Create timestamped filename
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd-HH-mm-ss");
    QString filename = QString("auto-backup-%1.kotodama.json").arg(timestamp);
    QString backupPath = backupDir + "/" + filename;

    // Create the backup
    result = exportToFile(backupPath);

    // Store backup status in settings
    QSettings settings;
    settings.setValue("autoBackup/lastBackupTime", QDateTime::currentDateTime().toString(Qt::ISODate));
    settings.setValue("autoBackup/lastBackupStatus", result.success);

    if (result.success) {
        // Reset warning flag on successful backup
        settings.setValue("autoBackup/warningShown", false);
    } else {
        settings.setValue("autoBackup/lastBackupError", result.errorMessage);
        // Reset warning flag so it will be shown on next launch
        settings.setValue("autoBackup/warningShown", false);
    }

    return result;
}

void BackupManager::rotateBackups(int maxBackups)
{
    if (maxBackups <= 0) {
        return;
    }

    QString backupDir = getBackupDirectory();
    QDir dir(backupDir);

    if (!dir.exists()) {
        return;  // No backups directory, nothing to rotate
    }

    // Get all backup files, sorted by name (which includes timestamp)
    QStringList filters;
    filters << "auto-backup-*.kotodama.json";
    QFileInfoList backupFiles = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    // If we have more backups than the limit, delete the oldest ones
    int backupsToDelete = backupFiles.size() - maxBackups;
    if (backupsToDelete > 0) {
        for (int i = 0; i < backupsToDelete; ++i) {
            QFile::remove(backupFiles[i].absoluteFilePath());
        }
    }
}
