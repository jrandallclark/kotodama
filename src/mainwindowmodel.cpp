#include "kotodama/mainwindowmodel.h"
#include "kotodama/databasemanager.h"
#include "kotodama/librarymanager.h"
#include "kotodama/languagemanager.h"
#include "kotodama/termmanager.h"
#include "kotodama/progresscalculator.h"

#include <QFileInfo>
#include <QFile>
#include <QTextStream>

MainWindowModel::MainWindowModel()
{
}

QList<TextDisplayItem> MainWindowModel::getTexts(const QString& languageCode)
{
    QList<TextInfo> textInfos = DatabaseManager::instance().getTexts(languageCode);
    QList<TextDisplayItem> items;

    for (const TextInfo& info : textInfos) {
        items.append(convertTextInfo(info));
    }

    return items;
}

int MainWindowModel::getTextCount(const QString& languageCode)
{
    return getTexts(languageCode).size();
}

TextDisplayItem MainWindowModel::getTextByUuid(const QString& uuid)
{
    TextInfo info = DatabaseManager::instance().getText(uuid);
    return convertTextInfo(info);
}

QString MainWindowModel::getTextFilePath(const QString& uuid)
{
    return LibraryManager::instance().getTextFilePath(uuid);
}

bool MainWindowModel::updateLastOpened(const QString& uuid)
{
    DatabaseManager::instance().updateLastOpened(uuid);
    return true;
}

bool MainWindowModel::deleteText(const QString& uuid)
{
    // Delete from database
    bool dbSuccess = DatabaseManager::instance().deleteText(uuid);

    // Delete file from library
    bool fileSuccess = LibraryManager::instance().deleteText(uuid);

    // Clear progress cache for this text
    ProgressCalculator::instance().removeFromCache(uuid);

    return dbSuccess && fileSuccess;
}

ImportTextResult MainWindowModel::importText(const QString& sourceFilePath,
                                             const QString& title,
                                             const QString& languageCode)
{
    ImportTextResult result;
    result.success = false;

    // Validate inputs
    if (sourceFilePath.isEmpty() || title.isEmpty() || languageCode.isEmpty()) {
        result.errorMessage = "Invalid input parameters";
        return result;
    }

    // Import file to library
    QString uuid = LibraryManager::instance().importText(sourceFilePath);

    if (uuid.isEmpty()) {
        result.errorMessage = "Failed to import file to library";
        return result;
    }

    // Get file metadata
    qint64 fileSize = LibraryManager::instance().getFileSize(sourceFilePath);
    int charCount = LibraryManager::instance().getCharacterCount(sourceFilePath);

    // Validate file is not empty
    if (charCount == 0) {
        result.errorMessage = "Cannot import empty file. The file contains no text.";
        // Clean up imported file
        LibraryManager::instance().deleteText(uuid);
        return result;
    }

    // Check if file contains only whitespace
    QFile file(sourceFilePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString content = in.readAll();
        file.close();

        if (content.trimmed().isEmpty()) {
            result.errorMessage = "Cannot import file with only whitespace. The file must contain actual text.";
            // Clean up imported file
            LibraryManager::instance().deleteText(uuid);
            return result;
        }
    }

    QFileInfo fileInfo(sourceFilePath);
    QString originalFilename = fileInfo.fileName();

    // Add to database
    QString addedUuid = DatabaseManager::instance().addText(
        uuid,
        title,
        originalFilename,
        languageCode,
        fileSize,
        charCount
    );

    if (addedUuid.isEmpty()) {
        result.errorMessage = "Failed to add text to database";
        // Clean up imported file
        LibraryManager::instance().deleteText(uuid);
        return result;
    }

    result.success = true;
    result.uuid = uuid;
    return result;
}

MainWindowModel::VocabImportResult MainWindowModel::importVocabulary(
    const QList<VocabImportItem>& items, bool updateDuplicates)
{
    VocabImportResult result;
    result.successCount = 0;
    result.failCount = 0;

    // Convert to TermManager format
    QList<TermManager::TermToImport> termsToImport;
    for (const VocabImportItem& item : items) {
        TermManager::TermToImport term;
        term.term = item.term;
        term.language = item.language;
        term.level = item.level;
        term.definition = item.definition;
        term.pronunciation = item.pronunciation;
        termsToImport.append(term);
    }

    // Batch import with update flag
    TermManager::ImportResult importResult = TermManager::instance().batchAddTerms(termsToImport, updateDuplicates);

    result.successCount = importResult.successCount;
    result.failCount = importResult.failCount;
    result.errors = importResult.errors;

    return result;
}

TextDisplayItem MainWindowModel::convertTextInfo(const TextInfo& info)
{
    TextDisplayItem item;
    item.uuid = info.uuid;
    item.title = info.title;
    item.language = info.language;
    // Get language name from LanguageManager
    LanguageConfig config = LanguageManager::instance().getLanguageByCode(info.language);
    item.languageName = config.name().isEmpty() ? info.language : config.name();
    item.characterCount = info.characterCount;
    item.addedDate = info.addedDate;
    item.lastOpened = info.lastOpened;
    return item;
}

TextProgressStats MainWindowModel::calculateTextProgress(const QString& uuid)
{
    return ProgressCalculator::instance().calculateTextProgress(uuid);
}

void MainWindowModel::invalidateProgressCache(const QString& language)
{
    ProgressCalculator::instance().invalidateProgressCache(language);
}
