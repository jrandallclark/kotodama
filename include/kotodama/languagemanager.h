#ifndef LANGUAGEMANAGER_H
#define LANGUAGEMANAGER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include "languageconfig.h"

class LanguageManager
{
public:
    static LanguageManager& instance();

    // Language retrieval
    LanguageConfig getLanguageByCode(const QString& code);
    LanguageConfig getLanguageByName(const QString& name);
    QList<LanguageConfig> getAllLanguages();
    QStringList getAllLanguageCodes();
    QStringList getAllLanguageNames(); 

    // Custom language management
    bool addCustomLanguage(const LanguageConfig& config);
    bool updateCustomLanguage(const QString& oldCode, const LanguageConfig& newConfig);
    bool deleteCustomLanguage(const QString& code);
    bool isBuiltIn(const QString& code);

    // True for languages that need an on-demand support module (e.g. "ja"
    // requires the MeCab IPA dictionary). Used by UI to gate import/open.
    static bool languageRequiresModule(const QString& code);

    // Validation
    bool validateLanguageCode(const QString& code, const QString& excludeCode = "");
    bool validateRegexPattern(const QString& regex);

    // Legacy API compatibility
    QString getWordRegex(const QString& code);
    bool isCharacterBased(const QString& code);

    // Reload custom languages from database
    void reloadCustomLanguages();

private:
    LanguageManager();
    ~LanguageManager();
    LanguageManager(const LanguageManager&) = delete;
    LanguageManager& operator=(const LanguageManager&) = delete;

    void loadBuiltIns();
    void loadCustomLanguages();

    QMap<QString, LanguageConfig> builtInLanguages;  // en, fr, ja
    QMap<QString, LanguageConfig> customLanguages;    // loaded from DB
};

#endif // LANGUAGEMANAGER_H
