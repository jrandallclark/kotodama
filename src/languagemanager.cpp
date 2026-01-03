#include "kotodama/languagemanager.h"
#include "kotodama/databasemanager.h"
#include <QRegularExpression>
#include <QDebug>

LanguageManager& LanguageManager::instance()
{
    static LanguageManager instance;
    return instance;
}

LanguageManager::LanguageManager()
{
    loadBuiltIns();
    loadCustomLanguages();
}

LanguageManager::~LanguageManager()
{
}

void LanguageManager::loadBuiltIns()
{
    // English
    builtInLanguages["en"] = LanguageConfig(
        "English",
        "en",
        "[-'a-zA-Z]+",
        false,
        6
    );

    // French
    builtInLanguages["fr"] = LanguageConfig(
        "French",
        "fr",
        "[-'\\x{2019}a-zA-ZÀ-ÖØ-ö\\x{00F8}-\\x{01BF}\\x{01C4}-\\x{024F}\\x{0370}-\\x{052F}\\x{1E00}-\\x{1FFF}]+",
        false,
        6
    );

    // Japanese
    builtInLanguages["ja"] = LanguageConfig(
        "Japanese",
        "ja",
        "[\\x{3005}\\x{3040}-\\x{309F}\\x{30A0}-\\x{30FF}\\x{4E00}-\\x{9FFF}]+",
        true,
        12
    );

    // Arabic
    builtInLanguages["ar"] = LanguageConfig(
        "Arabic",
        "ar",
        "[\\x{0600}-\\x{06FF}\\x{0750}-\\x{077F}\\x{08A0}-\\x{08FF}]+",
        false,
        6
    );

    // Spanish
    builtInLanguages["es"] = LanguageConfig(
        "Spanish",
        "es",
        "[-'\\x{2019}a-zA-ZÁ-ÿ]+",
        false,
        6
    );

    // Chinese (Mandarin)
    builtInLanguages["zh"] = LanguageConfig(
        "Chinese",
        "zh",
        "[\\x{4E00}-\\x{9FFF}]+",
        true,
        12
    );

    // Hindi
    builtInLanguages["hi"] = LanguageConfig(
        "Hindi",
        "hi",
        "[\\x{0900}-\\x{097F}]+",
        false,
        6
    );

    // Portuguese
    builtInLanguages["pt"] = LanguageConfig(
        "Portuguese",
        "pt",
        "[-'\\x{2019}a-zA-ZÀ-ÿ]+",
        false,
        6
    );

    // Russian
    builtInLanguages["ru"] = LanguageConfig(
        "Russian",
        "ru",
        "[\\x{0400}-\\x{04FF}]+",
        false,
        6
    );

    // German
    builtInLanguages["de"] = LanguageConfig(
        "German",
        "de",
        "[-'a-zA-ZÄÖÜäöüß]+",
        false,
        6
    );

    // Korean
    builtInLanguages["ko"] = LanguageConfig(
        "Korean",
        "ko",
        "[\\x{AC00}-\\x{D7AF}\\x{1100}-\\x{11FF}]+",
        true,
        12
    );

    // Italian
    builtInLanguages["it"] = LanguageConfig(
        "Italian",
        "it",
        "[-'\\x{2019}a-zA-ZÀ-ÿ]+",
        false,
        6
    );

    // Turkish
    builtInLanguages["tr"] = LanguageConfig(
        "Turkish",
        "tr",
        "[-'a-zA-ZÇçĞğİıÖöŞşÜü]+",
        false,
        6
    );

    // Polish
    builtInLanguages["pl"] = LanguageConfig(
        "Polish",
        "pl",
        "[-'a-zA-ZĄąĆćĘęŁłŃńÓóŚśŹźŻż]+",
        false,
        6
    );

    // Dutch
    builtInLanguages["nl"] = LanguageConfig(
        "Dutch",
        "nl",
        "[-'a-zA-ZÀ-ÿ]+",
        false,
        6
    );

    // Thai
    builtInLanguages["th"] = LanguageConfig(
        "Thai",
        "th",
        "[\\x{0E00}-\\x{0E7F}]+",
        true,
        12
    );

    // Persian (Farsi)
    builtInLanguages["fa"] = LanguageConfig(
        "Persian",
        "fa",
        "[\\x{0600}-\\x{06FF}\\x{0750}-\\x{077F}\\x{FB50}-\\x{FDFF}\\x{FE70}-\\x{FEFF}]+",
        false,
        6
    );

    // Vietnamese
    builtInLanguages["vi"] = LanguageConfig(
        "Vietnamese",
        "vi",
        "[-'a-zA-Z\\x{00C0}-\\x{1EF9}]+",
        false,
        6
    );

    // Swedish
    builtInLanguages["sv"] = LanguageConfig(
        "Swedish",
        "sv",
        "[-'a-zA-ZÅåÄäÖö]+",
        false,
        6
    );

    // Hebrew
    builtInLanguages["he"] = LanguageConfig(
        "Hebrew",
        "he",
        "[\\x{0590}-\\x{05FF}]+",
        false,
        6
    );
}

void LanguageManager::loadCustomLanguages()
{
    customLanguages.clear();
    QList<LanguageConfig> configs = DatabaseManager::instance().getLanguageConfigs();

    for (const LanguageConfig& config : configs) {
        // Only load non-builtin languages as custom
        if (!builtInLanguages.contains(config.code())) {
            customLanguages[config.code()] = config;
        }
    }
}

LanguageConfig LanguageManager::getLanguageByCode(const QString& code)
{
    // Check built-ins first
    if (builtInLanguages.contains(code)) {
        return builtInLanguages[code];
    }

    // Check custom languages
    if (customLanguages.contains(code)) {
        return customLanguages[code];
    }

    // Return empty config if not found
    return LanguageConfig();
}

LanguageConfig LanguageManager::getLanguageByName(const QString& name)
{
    // Check built-ins first
    for (const LanguageConfig& config : builtInLanguages) {
        if (config.name() == name) {
            return config;
        }
    }

    // Check custom languages
    for (const LanguageConfig& config : customLanguages) {
        if (config.name() == name) {
            return config;
        }
    }

    // Return empty config if not found
    return LanguageConfig();
}

QList<LanguageConfig> LanguageManager::getAllLanguages()
{
    QList<LanguageConfig> languages;

    // Add built-ins first (in order: en, fr, ja, ar, es, zh, hi, pt, ru, de, ko, it, tr, pl, nl, th, fa, vi, sv, he)
    QStringList builtInOrder = {"en", "fr", "ja", "ar", "es", "zh", "hi", "pt", "ru", "de", "ko", "it", "tr", "pl", "nl", "th", "fa", "vi", "sv", "he"};
    for (const QString& code : builtInOrder) {
        if (builtInLanguages.contains(code)) {
            languages.append(builtInLanguages[code]);
        }
    }

    // Add custom languages (sorted by code)
    QStringList customCodes = customLanguages.keys();
    customCodes.sort();
    for (const QString& code : customCodes) {
        languages.append(customLanguages[code]);
    }

    return languages;
}

QStringList LanguageManager::getAllLanguageCodes()
{
    QStringList codes;

    // Add built-ins first
    QStringList builtInOrder = {"en", "fr", "ja", "ar", "es", "zh", "hi", "pt", "ru", "de", "ko", "it", "tr", "pl", "nl", "th", "fa", "vi", "sv", "he"};
    for (const QString& code : builtInOrder) {
        if (builtInLanguages.contains(code)) {
            codes.append(code);
        }
    }

    // Add custom language codes (sorted)
    QStringList customCodes = customLanguages.keys();
    customCodes.sort();
    codes.append(customCodes);

    return codes;
}

QStringList LanguageManager::getAllLanguageNames()
{
    QStringList names;
    QList<LanguageConfig> languages = getAllLanguages();

    for (const LanguageConfig& config : languages) {
        names.append(config.name());
    }

    return names;
}

bool LanguageManager::addCustomLanguage(const LanguageConfig& config)
{
    // Validate
    if (!validateLanguageCode(config.code())) {
        qDebug() << "Invalid language code:" << config.code();
        return false;
    }

    if (!validateRegexPattern(config.wordRegex())) {
        qDebug() << "Invalid regex pattern:" << config.wordRegex();
        return false;
    }

    if (config.name().isEmpty()) {
        qDebug() << "Language name cannot be empty";
        return false;
    }

    // Check if code already exists
    if (builtInLanguages.contains(config.code()) || customLanguages.contains(config.code())) {
        qDebug() << "Language code already exists:" << config.code();
        return false;
    }

    // Add to database
    if (!DatabaseManager::instance().addLanguageConfig(config)) {
        qDebug() << "Failed to add language to database";
        return false;
    }

    // Add to cache
    customLanguages[config.code()] = config;

    return true;
}

bool LanguageManager::updateCustomLanguage(const QString& oldCode, const LanguageConfig& newConfig)
{
    // Cannot update built-in languages
    if (isBuiltIn(oldCode)) {
        qDebug() << "Cannot update built-in language:" << oldCode;
        return false;
    }

    // Validate new config
    if (!validateLanguageCode(newConfig.code(), oldCode)) {
        qDebug() << "Invalid language code:" << newConfig.code();
        return false;
    }

    if (!validateRegexPattern(newConfig.wordRegex())) {
        qDebug() << "Invalid regex pattern:" << newConfig.wordRegex();
        return false;
    }

    if (newConfig.name().isEmpty()) {
        qDebug() << "Language name cannot be empty";
        return false;
    }

    // Update in database
    if (!DatabaseManager::instance().updateLanguageConfig(oldCode, newConfig)) {
        qDebug() << "Failed to update language in database";
        return false;
    }

    // Update cache
    if (oldCode != newConfig.code()) {
        customLanguages.remove(oldCode);
    }
    customLanguages[newConfig.code()] = newConfig;

    return true;
}

bool LanguageManager::deleteCustomLanguage(const QString& code)
{
    // Cannot delete built-in languages
    if (isBuiltIn(code)) {
        qDebug() << "Cannot delete built-in language:" << code;
        return false;
    }

    // Check if exists
    if (!customLanguages.contains(code)) {
        qDebug() << "Language not found:" << code;
        return false;
    }

    // Delete from database
    if (!DatabaseManager::instance().deleteLanguageConfig(code)) {
        qDebug() << "Failed to delete language from database";
        return false;
    }

    // Remove from cache
    customLanguages.remove(code);

    return true;
}

bool LanguageManager::isBuiltIn(const QString& code)
{
    return builtInLanguages.contains(code);
}

bool LanguageManager::validateLanguageCode(const QString& code, const QString& excludeCode)
{
    // Format: 2-3 lowercase letters
    QRegularExpression codeRegex("^[a-z]{2,3}$");
    if (!codeRegex.match(code).hasMatch()) {
        return false;
    }

    // Check uniqueness (unless this is the code we're updating)
    if (code != excludeCode) {
        if (builtInLanguages.contains(code) || customLanguages.contains(code)) {
            return false;
        }
    }

    return true;
}

bool LanguageManager::validateRegexPattern(const QString& regex)
{
    if (regex.isEmpty()) {
        return false;
    }

    // Test if regex compiles
    QRegularExpression testRegex(regex);
    if (!testRegex.isValid()) {
        return false;
    }

    return true;
}

QString LanguageManager::getWordRegex(const QString& code)
{
    LanguageConfig config = getLanguageByCode(code);
    if (config.code().isEmpty()) {
        // Fallback to English-like pattern
        return "[-'a-zA-Z]+";
    }
    return config.wordRegex();
}

bool LanguageManager::isCharacterBased(const QString& code)
{
    LanguageConfig config = getLanguageByCode(code);
    if (config.code().isEmpty()) {
        // Default to false
        return false;
    }
    return config.isCharBased();
}

void LanguageManager::reloadCustomLanguages()
{
    loadCustomLanguages();
}
