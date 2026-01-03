#include "kotodama/databasemanager.h"
#include "kotodama/languageconfig.h"
#include "kotodama/languagemanager.h"
#include "kotodama/tokenizer.h"
#include "kotodama/constants.h"

#include "kotodama/term.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QRegularExpression>

DatabaseManager &DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager()
{
}

QSqlDatabase& DatabaseManager::database()
{
    return db;
}

DatabaseManager::~DatabaseManager()
{
    if (db.isOpen())
    {
        db.close();
    }

    db = QSqlDatabase();

    if (QSqlDatabase::contains(CONNECTION_NAME))
    {
        QSqlDatabase::removeDatabase(CONNECTION_NAME);
    }
}

bool DatabaseManager::initialize()
{
    // If already initialized, just return success
    if (db.isOpen())
    {
        return true;
    }

    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir;
    if (!dir.exists(dataPath))
    {
        dir.mkpath(dataPath);
    }

    QString dbPath = dataPath + "/database.sqlite";

    if (QSqlDatabase::contains(CONNECTION_NAME))
    {
        QSqlDatabase::removeDatabase(CONNECTION_NAME);
    }

    db = QSqlDatabase::addDatabase("QSQLITE", CONNECTION_NAME);
    db.setDatabaseName(dbPath);

    if (!db.open())
    {
        qDebug() << "Error opening database:" << db.lastError().text();
        return false;
    }

    // Enable WAL mode for better concurrency and reduced corruption risk
    QSqlQuery walQuery(db);
    if (!walQuery.exec("PRAGMA journal_mode=WAL"))
    {
        qDebug() << "Warning: Failed to enable WAL mode:" << walQuery.lastError().text();
        // Continue anyway - WAL mode is optional
    }

    // Enable foreign key constraints
    QSqlQuery fkQuery(db);
    if (!fkQuery.exec("PRAGMA foreign_keys=ON"))
    {
        qDebug() << "Warning: Failed to enable foreign keys:" << fkQuery.lastError().text();
    }

    return createTables();
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(db);

    QString createTextsTable = R"(
        CREATE TABLE IF NOT EXISTS texts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uuid TEXT UNIQUE NOT NULL,
            title TEXT NOT NULL,
            original_filename TEXT,
            language TEXT NOT NULL,
            file_size INTEGER,
            character_count INTEGER,
            added_date TEXT,
            last_opened TEXT,
            reading_position INTEGER DEFAULT 0,
            current_page INTEGER DEFAULT 1,
            total_pages INTEGER DEFAULT 0
        )
    )";

    if (!query.exec(createTextsTable))
    {
        qDebug() << "Error creating texts table:" << query.lastError().text();
        return false;
    }

    QString createTermsTable = R"(
        CREATE TABLE IF NOT EXISTS terms (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            term TEXT NOT NULL,
            language TEXT NOT NULL,
            level INTEGER NOT NULL,
            definition TEXT,
            pronunciation TEXT,
            added_date TEXT,
            last_modified TEXT,
            token_count INTEGER DEFAULT 1,
            UNIQUE(term, language)
        )
    )";

    if (!query.exec(createTermsTable))
    {
        qDebug() << "Error creating terms table:" << query.lastError().text();
        return false;
    }

    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_texts_language ON texts(language)"))
    {
        qDebug() << "Error creating texts language index:" << query.lastError().text();
    }

    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_texts_last_opened ON texts(last_opened)"))
    {
        qDebug() << "Error creating texts last_opened index:" << query.lastError().text();
    }

    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_terms_language ON terms(language)"))
    {
        qDebug() << "Error creating terms language index:" << query.lastError().text();
    }

    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_terms_language_level ON terms(language, level)"))
    {
        qDebug() << "Error creating terms language+level index:" << query.lastError().text();
    }

    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_terms_term ON terms(term)"))
    {
        qDebug() << "Error creating terms term index:" << query.lastError().text();
    }

    QString createLanguageConfigsTable = R"(
        CREATE TABLE IF NOT EXISTS language_configs (
            code TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            word_regex TEXT NOT NULL,
            is_char_based BOOL NOT NULL,
            is_builtin BOOL DEFAULT 0,
            token_limit INTEGER NOT NULL DEFAULT 6
        )
    )";

    if (!query.exec(createLanguageConfigsTable)) {
        qDebug() << "Error creating language_configs table:" << query.lastError().text();
        return false;
    }

    // Insert built-in languages if they don't exist
    QStringList builtInCodes = {"en", "fr", "ja", "ar"};
    QSqlQuery checkQuery(db);
    for (const QString& code : builtInCodes) {
        checkQuery.prepare("SELECT code FROM language_configs WHERE code = :code");
        checkQuery.bindValue(":code", code);
        checkQuery.exec();

        if (!checkQuery.next()) {
            // Built-in doesn't exist, insert it
            QString name, regex;
            int tokenLimit;
            bool isCharBased = false;

            if (code == "en") {
                name = "English";
                regex = "[-'a-zA-Z]+";
                tokenLimit = 6;
            } else if (code == "fr") {
                name = "French";
                regex = "[-'\\x{2019}a-zA-ZÀ-ÖØ-ö\\x{00F8}-\\x{01BF}\\x{01C4}-\\x{024F}\\x{0370}-\\x{052F}\\x{1E00}-\\x{1FFF}]+";
                tokenLimit = 6;
            } else if (code == "ja") {
                name = "Japanese";
                regex = "[\\x{3040}-\\x{309F}\\x{30A0}-\\x{30FF}\\x{4E00}-\\x{9FFF}]+";
                isCharBased = true;
                tokenLimit = Constants::Term::DEFAULT_TOKEN_LIMIT;
            } else if (code == "ar") {
                name = "Arabic";
                regex = "[\\x{0600}-\\x{06FF}\\x{0750}-\\x{077F}\\x{08A0}-\\x{08FF}]+";
                tokenLimit = 6;
            }

            QSqlQuery insertQuery(db);
            insertQuery.prepare("INSERT INTO language_configs (code, name, word_regex, is_char_based, is_builtin, token_limit) VALUES (:code, :name, :regex, :char_based, 1, :token_limit)");
            insertQuery.bindValue(":code", code);
            insertQuery.bindValue(":name", name);
            insertQuery.bindValue(":regex", regex);
            insertQuery.bindValue(":char_based", isCharBased);
            insertQuery.bindValue(":token_limit", tokenLimit);

            if (!insertQuery.exec()) {
                qDebug() << "Error inserting built-in language" << code << ":" << insertQuery.lastError().text();
            }
        }
    }

    // Create user_languages table for tracking which languages the user is learning
    QString createUserLanguagesTable = R"(
        CREATE TABLE IF NOT EXISTS user_languages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            language_code TEXT NOT NULL UNIQUE,
            display_order INTEGER NOT NULL DEFAULT 0,
            added_date TEXT NOT NULL
        )
    )";

    if (!query.exec(createUserLanguagesTable)) {
        qDebug() << "Error creating user_languages table:" << query.lastError().text();
        return false;
    }

    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_user_languages_order ON user_languages(display_order)")) {
        qDebug() << "Error creating user_languages display_order index:" << query.lastError().text();
    }

    return true;
}

QString DatabaseManager::addText(const QString &uuid,
                                 const QString &title,
                                 const QString &originalFilename,
                                 const QString &language,
                                 qint64 fileSize,
                                 int characterCount)
{
    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO texts (uuid, title, original_filename, language,
                          file_size, character_count, added_date)
        VALUES (:uuid, :title, :original_filename, :language,
                :file_size, :character_count, :added_date)
    )");

    query.bindValue(":uuid", uuid);
    query.bindValue(":title", title);
    query.bindValue(":original_filename", originalFilename);
    query.bindValue(":language", language);
    query.bindValue(":file_size", fileSize);
    query.bindValue(":character_count", characterCount);
    query.bindValue(":added_date", QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!query.exec())
    {
        qDebug() << "Error adding text:" << query.lastError().text();
        return QString();
    }

    return uuid;
}

QList<TextInfo> DatabaseManager::getTexts(const QString &language)
{
    QList<TextInfo> texts;
    QSqlQuery query(db);

    if (language.isEmpty())
    {
        query.prepare("SELECT * FROM texts ORDER BY last_opened DESC, added_date DESC");
    }
    else
    {
        query.prepare("SELECT * FROM texts WHERE language = :language ORDER BY last_opened DESC, added_date DESC");
        query.bindValue(":language", language);
    }

    if (!query.exec())
    {
        qDebug() << "Error getting texts:" << query.lastError().text();
        return texts;
    }

    while (query.next())
    {
        TextInfo info;
        info.id = query.value("id").toInt();
        info.uuid = query.value("uuid").toString();
        info.title = query.value("title").toString();
        info.originalFilename = query.value("original_filename").toString();
        info.language = query.value("language").toString();
        info.fileSize = query.value("file_size").toLongLong();
        info.characterCount = query.value("character_count").toInt();
        info.addedDate = query.value("added_date").toString();
        info.lastOpened = query.value("last_opened").toString();
        info.readingPosition = query.value("reading_position").toInt();
        info.currentPage = query.value("current_page").toInt();
        info.totalPages = query.value("total_pages").toInt();

        texts.append(info);
    }

    return texts;
}

TextInfo DatabaseManager::getText(const QString &uuid)
{
    TextInfo info;
    QSqlQuery query(db);
    query.prepare("SELECT * FROM texts WHERE uuid = :uuid");
    query.bindValue(":uuid", uuid);

    if (query.exec() && query.next())
    {
        info.id = query.value("id").toInt();
        info.uuid = query.value("uuid").toString();
        info.title = query.value("title").toString();
        info.originalFilename = query.value("original_filename").toString();
        info.language = query.value("language").toString();
        info.fileSize = query.value("file_size").toLongLong();
        info.characterCount = query.value("character_count").toInt();
        info.addedDate = query.value("added_date").toString();
        info.lastOpened = query.value("last_opened").toString();
        info.readingPosition = query.value("reading_position").toInt();
        info.currentPage = query.value("current_page").toInt();
        info.totalPages = query.value("total_pages").toInt();
    }

    return info;
}

bool DatabaseManager::deleteText(const QString &uuid)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM texts WHERE uuid = :uuid");
    query.bindValue(":uuid", uuid);

    if (!query.exec())
    {
        qDebug() << "Error deleting text:" << query.lastError().text();
        return false;
    }

    return true;
}

void DatabaseManager::updateReadingPosition(const QString &uuid, int position)
{
    QSqlQuery query(db);
    query.prepare("UPDATE texts SET reading_position = :position WHERE uuid = :uuid");
    query.bindValue(":position", position);
    query.bindValue(":uuid", uuid);

    if (!query.exec())
    {
        qDebug() << "Error updating reading position:" << query.lastError().text();
    }
}

void DatabaseManager::updatePageInfo(const QString &uuid, int currentPage, int totalPages)
{
    QSqlQuery query(db);
    query.prepare("UPDATE texts SET current_page = :current, total_pages = :total WHERE uuid = :uuid");
    query.bindValue(":current", currentPage);
    query.bindValue(":total", totalPages);
    query.bindValue(":uuid", uuid);

    if (!query.exec())
    {
        qDebug() << "Error updating page info:" << query.lastError().text();
    }
}

void DatabaseManager::updateLastOpened(const QString &uuid)
{
    QSqlQuery query(db);
    query.prepare("UPDATE texts SET last_opened = :date WHERE uuid = :uuid");
    query.bindValue(":date", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":uuid", uuid);

    if (!query.exec())
    {
        qDebug() << "Error updating last opened:" << query.lastError().text();
    }
}

QString DatabaseManager::getTextFilePath(const QString &uuid)
{
    QString libraryPath = getLibraryPath();
    return libraryPath + "/texts/" + uuid + ".txt";
}

QString DatabaseManager::getLibraryPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

bool DatabaseManager::addTerm(const QString &term,
                              const QString &language,
                              TermLevel level,
                              const QString &definition,
                              const QString &pronunciation)
{
    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO terms (term, language, level, definition, pronunciation, added_date, last_modified, token_count)
        VALUES (:term, :language, :level, :definition, :pronunciation, :added_date, :last_modified, :token_count)
    )");

    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    // Use LanguageManager for fallback-aware regex (not raw LanguageConfig which may be empty)
    QString dbRegex = LanguageManager::instance().getWordRegex(language);
    bool dbCharBased = LanguageManager::instance().isCharacterBased(language);
    auto dbTok = Tokenizer::createRegex(dbRegex, dbCharBased);
    auto dbTokenResults = dbTok->tokenize(term);
    std::vector<std::string> tokens;
    for (const auto& r : dbTokenResults) tokens.push_back(r.text);

    query.bindValue(":term", term);
    query.bindValue(":language", language);
    query.bindValue(":level", static_cast<int>(level));
    query.bindValue(":definition", definition);
    query.bindValue(":pronunciation", pronunciation);
    query.bindValue(":added_date", now);
    query.bindValue(":last_modified", now);
    query.bindValue(":token_count", static_cast<int>(tokens.size()));

    if (!query.exec())
    {
        qDebug() << "Error adding term:" << query.lastError().text();
        return false;
    }

    return true;
}

QList<Term> DatabaseManager::getTerms(const QString &language)
{
    QList<Term> terms;
    QSqlQuery query(db);

    if (language.isEmpty())
    {
        query.prepare("SELECT * FROM terms ORDER BY term");
    }
    else
    {
        query.prepare("SELECT * FROM terms WHERE language = :language ORDER BY term");
        query.bindValue(":language", language);
    }

    if (!query.exec())
    {
        qDebug() << "Error getting terms:" << query.lastError().text();
        return terms;
    }

    while (query.next())
    {
        Term t;
        t.id = query.value("id").toInt();
        t.term = query.value("term").toString();
        t.language = query.value("language").toString();
        t.level = static_cast<TermLevel>(query.value("level").toInt());
        t.definition = query.value("definition").toString();
        t.pronunciation = query.value("pronunciation").toString();
        t.addedDate = query.value("added_date").toString();
        t.lastModified = query.value("last_modified").toString();
        t.tokenCount = query.value("token_count").toInt();

        terms.append(t);
    }

    return terms;
}

QList<Term> DatabaseManager::getTermsByLevel(const QString &language, TermLevel level)
{
    QList<Term> terms;
    QSqlQuery query(db);

    query.prepare("SELECT * FROM terms WHERE language = :language AND level = :level ORDER BY term");
    query.bindValue(":language", language);
    query.bindValue(":level", static_cast<int>(level));

    if (!query.exec())
    {
        qDebug() << "Error getting terms by level:" << query.lastError().text();
        return terms;
    }

    while (query.next())
    {
        Term t;
        t.id = query.value("id").toInt();
        t.term = query.value("term").toString();
        t.language = query.value("language").toString();
        t.level = static_cast<TermLevel>(query.value("level").toInt());
        t.definition = query.value("definition").toString();
        t.pronunciation = query.value("pronunciation").toString();
        t.addedDate = query.value("added_date").toString();
        t.lastModified = query.value("last_modified").toString();
        t.tokenCount = query.value("token_count").toInt();

        terms.append(t);
    }

    return terms;
}

Term DatabaseManager::getTerm(const QString &term, const QString &language)
{
    Term t;
    QSqlQuery query(db);
    query.prepare("SELECT * FROM terms WHERE term = :term AND language = :language");
    query.bindValue(":term", term);
    query.bindValue(":language", language);

    if (query.exec() && query.next())
    {
        t.id = query.value("id").toInt();
        t.term = query.value("term").toString();
        t.language = query.value("language").toString();
        t.level = static_cast<TermLevel>(query.value("level").toInt());
        t.definition = query.value("definition").toString();
        t.pronunciation = query.value("pronunciation").toString();
        t.addedDate = query.value("added_date").toString();
        t.lastModified = query.value("last_modified").toString();
        t.tokenCount = query.value("token_count").toInt();
    }

    return t;
}

bool DatabaseManager::updateTerm(const QString &term,
                                 const QString &language,
                                 TermLevel level,
                                 const QString &definition,
                                 const QString &pronunciation)
{
    QSqlQuery query(db);
    query.prepare(R"(
        UPDATE terms
        SET level = :level, definition = :definition, pronunciation = :pronunciation, last_modified = :last_modified
        WHERE term = :term AND language = :language
    )");

    query.bindValue(":level", static_cast<int>(level));
    query.bindValue(":definition", definition);
    query.bindValue(":pronunciation", pronunciation);
    query.bindValue(":last_modified", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":term", term);
    query.bindValue(":language", language);

    if (!query.exec())
    {
        qDebug() << "Error updating term:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::updateTermLevel(const QString &term, const QString &language, TermLevel level)
{
    QSqlQuery query(db);
    query.prepare("UPDATE terms SET level = :level, last_modified = :last_modified WHERE term = :term AND language = :language");
    query.bindValue(":level", static_cast<int>(level));
    query.bindValue(":last_modified", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":term", term);
    query.bindValue(":language", language);

    if (!query.exec())
    {
        qDebug() << "Error updating term level:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::deleteTerm(const QString &term, const QString &language)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM terms WHERE term = :term AND language = :language");
    query.bindValue(":term", term);
    query.bindValue(":language", language);

    if (!query.exec())
    {
        qDebug() << "Error deleting term:" << query.lastError().text();
        return false;
    }

    return true;
}

int DatabaseManager::getTermCount(const QString &language)
{
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM terms WHERE language = :language");
    query.bindValue(":language", language);

    if (query.exec() && query.next())
    {
        return query.value(0).toInt();
    }

    return 0;
}

int DatabaseManager::getTermCountByLevel(const QString &language, TermLevel level)
{
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM terms WHERE language = :language AND level = :level");
    query.bindValue(":language", language);
    query.bindValue(":level", static_cast<int>(level));

    if (query.exec() && query.next())
    {
        return query.value(0).toInt();
    }

    return 0;
}

QList<LanguageConfig> DatabaseManager::getLanguageConfigs()
{

    QList<LanguageConfig> languageConfigs;
    QSqlQuery query(db);

    query.prepare("SELECT * FROM language_configs");

    if (!query.exec())
    {
        qDebug() << "Error getting language configs:" << query.lastError().text();
        return languageConfigs;
    }

    while (query.next())
    {
        languageConfigs.append(LanguageConfig(
            query.value("name").toString(),
            query.value("code").toString(),
            query.value("word_regex").toString(),
            query.value("is_char_based").toBool(),
            query.value("token_limit").toInt()
        ));
    }

    return languageConfigs;
}

bool DatabaseManager::addLanguageConfig(const LanguageConfig& config)
{
    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO language_configs (code, name, word_regex, is_char_based, is_builtin, token_limit) "
                  "VALUES (:code, :name, :regex, :char_based, 0, :token_limit)");
    query.bindValue(":code", config.code());
    query.bindValue(":name", config.name());
    query.bindValue(":regex", config.wordRegex());
    query.bindValue(":char_based", config.isCharBased());
    query.bindValue(":token_limit", config.tokenLimit());

    if (!query.exec())
    {
        qDebug() << "Error adding language config:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::updateLanguageConfig(const QString& oldCode, const LanguageConfig& newConfig)
{
    // If the code changed, we need to delete the old one and insert the new one
    if (oldCode != newConfig.code())
    {
        QSqlQuery query(db);
        query.prepare("DELETE FROM language_configs WHERE code = :old_code AND is_builtin = 0");
        query.bindValue(":old_code", oldCode);

        if (!query.exec())
        {
            qDebug() << "Error deleting old language config:" << query.lastError().text();
            return false;
        }
    }

    // Insert or update the new config
    return addLanguageConfig(newConfig);
}

bool DatabaseManager::deleteLanguageConfig(const QString& code)
{
    QSqlQuery query(db);
    // Only allow deletion of non-builtin languages
    query.prepare("DELETE FROM language_configs WHERE code = :code AND is_builtin = 0");
    query.bindValue(":code", code);

    if (!query.exec())
    {
        qDebug() << "Error deleting language config:" << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() == 0)
    {
        qDebug() << "Cannot delete built-in language or language not found:" << code;
        return false;
    }

    return true;
}

QStringList DatabaseManager::getUserLanguages()
{
    QStringList codes;
    QSqlQuery query(db);
    query.prepare("SELECT language_code FROM user_languages ORDER BY display_order, added_date");

    if (!query.exec())
    {
        qDebug() << "Error getting user languages:" << query.lastError().text();
        return codes;
    }

    while (query.next())
    {
        codes.append(query.value("language_code").toString());
    }

    return codes;
}

bool DatabaseManager::addUserLanguage(const QString& languageCode)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO user_languages (language_code, added_date, display_order) "
                  "VALUES (:code, :date, (SELECT COALESCE(MAX(display_order), -1) + 1 FROM user_languages))");
    query.bindValue(":code", languageCode);
    query.bindValue(":date", QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!query.exec())
    {
        qDebug() << "Error adding user language:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::removeUserLanguage(const QString& languageCode)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM user_languages WHERE language_code = :code");
    query.bindValue(":code", languageCode);

    if (!query.exec())
    {
        qDebug() << "Error removing user language:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool DatabaseManager::hasUserLanguages()
{
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) as count FROM user_languages");

    if (query.exec() && query.next())
    {
        return query.value("count").toInt() > 0;
    }

    return false;
}

int DatabaseManager::getUserLanguageCount()
{
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) as count FROM user_languages");

    if (query.exec() && query.next())
    {
        return query.value("count").toInt();
    }

    return 0;
}

IntegrityCheckResult DatabaseManager::checkIntegrity()
{
    IntegrityCheckResult result;

    if (!db.isOpen())
    {
        result.ok = false;
        result.errors.append("Database is not open");
        return result;
    }

    QSqlQuery query(db);
    if (!query.exec("PRAGMA integrity_check"))
    {
        result.ok = false;
        result.errors.append("Failed to run integrity check: " + query.lastError().text());
        return result;
    }

    // Collect all result rows
    while (query.next())
    {
        QString checkResult = query.value(0).toString();

        // SQLite returns "ok" if database is intact
        if (checkResult == "ok")
        {
            result.ok = true;
            return result;
        }

        // Otherwise, each row is an error message
        result.errors.append(checkResult);
    }

    // If we got here and have errors, database is corrupted
    if (!result.errors.isEmpty())
    {
        result.ok = false;
    }

    return result;
}


