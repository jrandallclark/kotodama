#ifndef PROGRESSCALCULATOR_H
#define PROGRESSCALCULATOR_H

#include <QString>
#include <QMap>

// Struct for text progress statistics
struct TextProgressStats {
    int totalUniqueWords;
    int knownWords;
    int newWords;
    float percentKnown;
};

// Singleton class for calculating and caching text progress statistics
class ProgressCalculator
{
public:
    // Singleton accessor
    static ProgressCalculator& instance();

    // Calculate progress statistics for a text
    TextProgressStats calculateTextProgress(const QString& uuid);

    // Invalidate cached progress for texts in a specific language
    void invalidateProgressCache(const QString& language);

    // Remove specific UUID from cache
    void removeFromCache(const QString& uuid);

    // Clear all cached progress
    void clearCache();

private:
    // Singleton pattern - private constructor
    ProgressCalculator();
    ~ProgressCalculator() = default;
    ProgressCalculator(const ProgressCalculator&) = delete;
    ProgressCalculator& operator=(const ProgressCalculator&) = delete;

    // Progress cache
    QMap<QString, TextProgressStats> progressCache;
};

#endif // PROGRESSCALCULATOR_H
