#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include "term.h"
#include "languageconfig.h"

#include <QString>
#include <QList>
#include <QSqlDatabase>

struct TextInfo {
    int id;
    QString uuid;
    QString title;
    QString originalFilename;
    QString language;
    qint64 fileSize;
    int characterCount;
    QString addedDate;
    QString lastOpened;
    int readingPosition;
    int currentPage;
    int totalPages;
};

struct IntegrityCheckResult {
    bool ok;
    QStringList errors;

    IntegrityCheckResult() : ok(true) {}
};

class DatabaseManager
{
public:
    static DatabaseManager& instance();

    bool initialize();
    QSqlDatabase& database();
    IntegrityCheckResult checkIntegrity();

    // Text management
    QString addText(const QString& uuid,
                    const QString& title,
                    const QString& originalFilename,
                    const QString& language,
                    qint64 fileSize,
                    int characterCount);

    QList<TextInfo> getTexts(const QString& language = "");
    TextInfo getText(const QString& uuid);
    bool deleteText(const QString& uuid);

    // Reading progress
    void updateReadingPosition(const QString& uuid, int position);
    void updatePageInfo(const QString& uuid, int currentPage, int totalPages);
    void updateLastOpened(const QString& uuid);

    QString getTextFilePath(const QString& uuid);

    bool addTerm(const QString& term,
                 const QString& language,
                 TermLevel level = TermLevel::Recognized,
                 const QString& definition = "",
                 const QString& pronunciation = "");

    QList<Term> getTerms(const QString& language = "");
    QList<Term> getTermsByLevel(const QString& language, TermLevel level);
    Term getTerm(const QString& term, const QString& language);

    bool updateTerm(const QString& term,
                    const QString& language,
                    TermLevel level,
                    const QString& definition,
                    const QString& pronunciation);

    bool updateTermLevel(const QString& term, const QString& language, TermLevel level);
    bool deleteTerm(const QString& term, const QString& language);

    int getTermCount(const QString& language);
    int getTermCountByLevel(const QString& language, TermLevel level);

    QList<LanguageConfig> getLanguageConfigs();

    // Language config management
    bool addLanguageConfig(const LanguageConfig& config);
    bool updateLanguageConfig(const QString& oldCode, const LanguageConfig& newConfig);
    bool deleteLanguageConfig(const QString& code);

    // User language management
    QStringList getUserLanguages();
    bool addUserLanguage(const QString& languageCode);
    bool removeUserLanguage(const QString& languageCode);
    bool hasUserLanguages();
    int getUserLanguageCount();

private:
    DatabaseManager();
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    static constexpr const char* CONNECTION_NAME = "kotodama_main";

    QSqlDatabase db;
    bool createTables();
    QString getLibraryPath();
};

#endif // DATABASEMANAGER_H
