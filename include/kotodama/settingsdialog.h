#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QColor>
#include <QMap>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:
    void onColorButtonClicked();
    void onResetColorsClicked();
    void onAccept();
    void onReject();

private:
    // Color buttons for each highlight level (light and dark)
    QMap<QString, QPushButton*> lightColorButtons;
    QMap<QString, QPushButton*> darkColorButtons;
    QMap<QString, QColor> lightColors;
    QMap<QString, QColor> darkColors;

    // Font size control
    QSpinBox* fontSizeSpinBox;

    // Theme control
    QComboBox* themeComboBox;

    // Backup settings controls
    QCheckBox* autoBackupCheckBox;
    QSpinBox* maxBackupsSpinBox;

    // Helper methods
    void loadColors();
    void saveColors();
    void loadFontSize();
    void saveFontSize();
    void loadTheme();
    void saveTheme();
    void loadBackupSettings();
    void saveBackupSettings();
    void updateButtonColor(QPushButton* button, const QColor& color);
    QColor getDefaultLightColor(const QString& level);
    QColor getDefaultDarkColor(const QString& level);
};

#endif // SETTINGSDIALOG_H
