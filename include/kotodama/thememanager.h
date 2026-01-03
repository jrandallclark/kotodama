#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QColor>
#include <QString>

enum class ThemeMode {
    Auto,
    Light,
    Dark
};

enum class ThemeColor {
    // Backgrounds
    WindowBackground,
    CardBackground,
    InputBackground,
    InputBackgroundDisabled,

    // Text
    TextPrimary,
    TextSecondary,

    // Borders and dividers
    Border,
    Divider,

    // Accents
    Primary,
    PrimaryHover,
    PrimaryPressed,

    // Term levels
    LevelRecognized,
    LevelLearning,
    LevelKnown,
    LevelWellKnown,
    LevelIgnored,
    LevelUnknown
};

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    static ThemeManager& instance();

    // Get current theme mode from settings
    ThemeMode getThemeMode() const;
    void setThemeMode(ThemeMode mode);

    // Check if currently in dark mode (considering Auto mode)
    bool isDarkMode() const;

    // Get color for current theme
    QColor getColor(ThemeColor color) const;
    QString getColorHex(ThemeColor color) const;

    // Detect system theme
    static bool isSystemDarkMode();

signals:
    void themeChanged();

private:
    ThemeManager();
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    QColor getLightColor(ThemeColor color) const;
    QColor getDarkColor(ThemeColor color) const;
};

#endif // THEMEMANAGER_H
