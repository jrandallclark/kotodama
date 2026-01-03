#ifndef MAINWINDOWMODEL_H
#define MAINWINDOWMODEL_H

#include "term.h"
#include "progresscalculator.h"
#include <QString>
#include <QList>

struct TextInfo;

// Struct for text display information
struct TextDisplayItem {
    QString uuid;
    QString title;
    QString language;
    QString languageName;
    int characterCount;
    QString addedDate;
    QString lastOpened;
};

// Struct for import result
struct ImportTextResult {
    bool success;
    QString uuid;
    QString errorMessage;
};

// Business logic for main window operations
class MainWindowModel
{
public:
    MainWindowModel();

    // Text list management
    QList<TextDisplayItem> getTexts(const QString& languageCode = "");
    int getTextCount(const QString& languageCode = "");

    // Text operations
    TextDisplayItem getTextByUuid(const QString& uuid);
    QString getTextFilePath(const QString& uuid);
    bool updateLastOpened(const QString& uuid);
    bool deleteText(const QString& uuid);

    // Progress calculation
    TextProgressStats calculateTextProgress(const QString& uuid);
    void invalidateProgressCache(const QString& language);

    // Import workflow
    ImportTextResult importText(const QString& sourceFilePath,
                                const QString& title,
                                const QString& languageCode);

    // Vocabulary import
    struct VocabImportItem {
        QString term;
        QString language;
        TermLevel level;
        QString definition;
        QString pronunciation;
    };

    struct VocabImportResult {
        int successCount;
        int failCount;
        QStringList errors;
    };

    VocabImportResult importVocabulary(const QList<VocabImportItem>& items, bool updateDuplicates = false);

private:
    // Helper to convert TextInfo to TextDisplayItem
    TextDisplayItem convertTextInfo(const TextInfo& info);
};

#endif // MAINWINDOWMODEL_H
