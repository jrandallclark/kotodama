#include "kotodama/progresscalculator.h"
#include "kotodama/databasemanager.h"
#include "kotodama/librarymanager.h"
#include "kotodama/ebookviewmodel.h"
#include "kotodama/languagemanager.h"
#include "kotodama/languagemodulemanager.h"

#include <QFile>
#include <QTextStream>

ProgressCalculator::ProgressCalculator()
{
}

ProgressCalculator& ProgressCalculator::instance()
{
    static ProgressCalculator instance;
    return instance;
}

TextProgressStats ProgressCalculator::calculateTextProgress(const QString& uuid)
{
    // Check cache first
    if (progressCache.contains(uuid)) {
        return progressCache[uuid];
    }

    // Initialize default stats
    TextProgressStats stats;
    stats.totalUniqueWords = 0;
    stats.knownWords = 0;
    stats.newWords = 0;
    stats.percentKnown = 0.0f;

    // Get file path
    QString filePath = LibraryManager::instance().getTextFilePath(uuid);
    if (filePath.isEmpty()) {
        progressCache[uuid] = stats;
        return stats;
    }

    // Read file content
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        progressCache[uuid] = stats;
        return stats;
    }

    QTextStream in(&file);
    QString text = in.readAll();
    file.close();

    if (text.isEmpty()) {
        progressCache[uuid] = stats;
        return stats;
    }

    // Get language from database
    TextInfo textInfo = DatabaseManager::instance().getText(uuid);
    QString language = textInfo.language;

    // Skip analysis for languages whose support module isn't installed yet.
    // The text card still appears in the library; the user just sees blank
    // progress until they install the module from Language Manager.
    // Don't cache the empty result — installing the module won't trigger
    // a rebuild, and we want the progress to populate on next access.
    if (LanguageManager::languageRequiresModule(language) &&
        !LanguageModuleManager::instance().isInstalled(language)) {
        return stats;
    }

    // Analyze text using EbookViewModel
    EbookViewModel viewModel;
    viewModel.loadContent(text, language);

    // Calculate stats using EbookViewModel
    stats = viewModel.calculateProgressStats();

    // Cache result
    progressCache[uuid] = stats;

    return stats;
}

void ProgressCalculator::invalidateProgressCache(const QString& language)
{
    // Clear all cached progress for texts in the specified language
    // We need to iterate and remove matching items
    QList<QString> toRemove;

    for (auto it = progressCache.begin(); it != progressCache.end(); ++it) {
        QString uuid = it.key();
        TextInfo textInfo = DatabaseManager::instance().getText(uuid);
        if (textInfo.language == language) {
            toRemove.append(uuid);
        }
    }

    for (const QString& uuid : toRemove) {
        progressCache.remove(uuid);
    }
}

void ProgressCalculator::removeFromCache(const QString& uuid)
{
    progressCache.remove(uuid);
}

void ProgressCalculator::clearCache()
{
    progressCache.clear();
}
