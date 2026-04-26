#include "kotodama/thememanager.h"
#include <QSettings>
#include <QApplication>
#include <QStyleHints>

ThemeManager& ThemeManager::instance()
{
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager()
{
}

ThemeMode ThemeManager::getThemeMode() const
{
    QSettings settings;
    QString modeStr = settings.value("theme/mode", "auto").toString();

    if (modeStr == "light") {
        return ThemeMode::Light;
    } else if (modeStr == "dark") {
        return ThemeMode::Dark;
    } else {
        return ThemeMode::Auto;
    }
}

void ThemeManager::setThemeMode(ThemeMode mode)
{
    QSettings settings;
    QString modeStr;

    switch (mode) {
    case ThemeMode::Light:
        modeStr = "light";
        break;
    case ThemeMode::Dark:
        modeStr = "dark";
        break;
    case ThemeMode::Auto:
        modeStr = "auto";
        break;
    }

    settings.setValue("theme/mode", modeStr);
    emit themeChanged();
}

bool ThemeManager::isDarkMode() const
{
    ThemeMode mode = getThemeMode();

    if (mode == ThemeMode::Dark) {
        return true;
    } else if (mode == ThemeMode::Light) {
        return false;
    } else {
        // Auto mode - check system
        return isSystemDarkMode();
    }
}

bool ThemeManager::isSystemDarkMode()
{
    Qt::ColorScheme scheme = QApplication::styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Dark;
}

QColor ThemeManager::getColor(ThemeColor color) const
{
    if (isDarkMode()) {
        return getDarkColor(color);
    } else {
        return getLightColor(color);
    }
}

QString ThemeManager::getColorHex(ThemeColor color) const
{
    return getColor(color).name();
}

QColor ThemeManager::getLightColor(ThemeColor color) const
{
    switch (color) {
    // Backgrounds
    case ThemeColor::WindowBackground:
        return QColor("#FAFAFA");  // Material Gray 50
    case ThemeColor::CardBackground:
        return QColor("#FFFFFF");  // White
    case ThemeColor::InputBackground:
        return QColor("#F5F5F5");  // Light gray
    case ThemeColor::InputBackgroundDisabled:
        return QColor("#FAFAFA");  // Very light gray

    // Text
    case ThemeColor::TextPrimary:
        return QColor("#212121");  // Almost black
    case ThemeColor::TextSecondary:
        return QColor("#757575");  // Medium gray

    // Borders and dividers
    case ThemeColor::Border:
        return QColor("#E0E0E0");  // Light gray
    case ThemeColor::Divider:
        return QColor("#E0E0E0");  // Light gray

    // Accents
    case ThemeColor::Primary:
        return QColor("#1976D2");  // Blue 700
    case ThemeColor::PrimaryHover:
        return QColor("#1565C0");  // Blue 800
    case ThemeColor::PrimaryPressed:
        return QColor("#0D47A1");  // Blue 900

    // Focus highlight (underline color for keyboard navigation)
    case ThemeColor::Focus:
        return QColor("#1976D2");  // Blue 700 (same as Primary)

    // Term levels
    case ThemeColor::LevelRecognized:
        return QColor("#66BB6A");  // Green 400 
    case ThemeColor::LevelLearning:
        return QColor("#81C784");  // Green 300 
    case ThemeColor::LevelKnown:
        return QColor("#A5D6A7");  // Green 200
    case ThemeColor::LevelWellKnown:
        return QColor(Qt::transparent);  // No highlight
    case ThemeColor::LevelIgnored:
        return QColor("#F5F5F5");  // Very light gray
    case ThemeColor::LevelUnknown:
        return QColor("#64B5F6");  // Blue 400

    default:
        return QColor("#FFFFFF");
    }
}

QColor ThemeManager::getDarkColor(ThemeColor color) const
{
    switch (color) {
    // Backgrounds
    case ThemeColor::WindowBackground:
        return QColor("#1E1E1E");  // Dark gray
    case ThemeColor::CardBackground:
        return QColor("#2D2D2D");  // Slightly lighter dark gray
    case ThemeColor::InputBackground:
        return QColor("#3A3A3A");  // Medium dark gray
    case ThemeColor::InputBackgroundDisabled:
        return QColor("#2D2D2D");  // Same as card

    // Text
    case ThemeColor::TextPrimary:
        return QColor("#E0E0E0");  // Light gray
    case ThemeColor::TextSecondary:
        return QColor("#A0A0A0");  // Medium gray

    // Borders and dividers
    case ThemeColor::Border:
        return QColor("#404040");  // Dark gray
    case ThemeColor::Divider:
        return QColor("#404040");  // Dark gray

    // Accents
    case ThemeColor::Primary:
        return QColor("#42A5F5");  // Blue 400
    case ThemeColor::PrimaryHover:
        return QColor("#64B5F6");  // Blue 300
    case ThemeColor::PrimaryPressed:
        return QColor("#90CAF9");  // Blue 200

    // Focus highlight (underline color for keyboard navigation)
    case ThemeColor::Focus:
        return QColor("#42A5F5");  // Blue 400 (same as Primary)

    // Term levels
    case ThemeColor::LevelRecognized:
        return QColor("#81C784");  // Green 300
    case ThemeColor::LevelLearning:
        return QColor("#A5D6A7");  // Green 200
    case ThemeColor::LevelKnown:
        return QColor("#C8E6C9");  // Green 100
    case ThemeColor::LevelWellKnown:
        return QColor(Qt::transparent);  // No highlight
    case ThemeColor::LevelIgnored:
        return QColor("#3A3A3A");  // Dark gray
    case ThemeColor::LevelUnknown:
        return QColor("#90CAF9");  // Blue 200 

    default:
        return QColor("#2D2D2D");
    }
}
