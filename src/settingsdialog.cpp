#include "kotodama/settingsdialog.h"
#include "kotodama/thememanager.h"
#include "kotodama/constants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QWidget>
#include <QLabel>
#include <QGroupBox>
#include <QColorDialog>
#include <QSettings>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QSpinBox>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Settings");
    setFixedSize(Constants::Dialog::SETTINGS_WIDTH, Constants::Dialog::SETTINGS_HEIGHT);

    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(Constants::Margins::SMALL, Constants::Margins::SMALL, Constants::Margins::SMALL, Constants::Margins::SMALL);
    mainLayout->setSpacing(Constants::Spacing::SMALL);

    // Create tab widget
    QTabWidget* tabWidget = new QTabWidget(this);
    mainLayout->addWidget(tabWidget);

    // Create General tab
    QWidget* generalTab = new QWidget();
    QVBoxLayout* generalLayout = new QVBoxLayout(generalTab);
    generalLayout->setContentsMargins(Constants::Margins::SMALL, Constants::Margins::SMALL, Constants::Margins::SMALL, Constants::Margins::SMALL);
    generalLayout->setSpacing(Constants::Spacing::SMALL);

    // Theme group
    QGroupBox* themeGroup = new QGroupBox("Appearance");
    QHBoxLayout* themeLayout = new QHBoxLayout(themeGroup);

    QLabel* themeLabel = new QLabel("Theme:");
    themeLayout->addWidget(themeLabel);

    themeComboBox = new QComboBox();
    themeComboBox->addItem("Auto (System)", "auto");
    themeComboBox->addItem("Light", "light");
    themeComboBox->addItem("Dark", "dark");
    loadTheme();
    themeLayout->addWidget(themeComboBox);

    themeLayout->addStretch();

    generalLayout->addWidget(themeGroup);

    // Backup group
    QGroupBox* backupGroup = new QGroupBox("Automatic Backups");
    QVBoxLayout* backupGroupLayout = new QVBoxLayout(backupGroup);

    autoBackupCheckBox = new QCheckBox("Create automatic backup on exit");
    backupGroupLayout->addWidget(autoBackupCheckBox);

    QHBoxLayout* maxBackupsLayout = new QHBoxLayout();
    QLabel* maxBackupsLabel = new QLabel("Keep last:");
    maxBackupsLayout->addWidget(maxBackupsLabel);

    maxBackupsSpinBox = new QSpinBox();
    maxBackupsSpinBox->setMinimum(1);
    maxBackupsSpinBox->setMaximum(10);
    maxBackupsSpinBox->setSuffix(" backups");
    maxBackupsLayout->addWidget(maxBackupsSpinBox);
    maxBackupsLayout->addStretch();

    backupGroupLayout->addLayout(maxBackupsLayout);

    QLabel* backupInfoLabel = new QLabel(
        "Automatic backups are stored in the application data directory and rotated when the limit is reached.");
    backupInfoLabel->setWordWrap(true);
    backupInfoLabel->setStyleSheet("color: gray; font-size: 11px;");
    backupGroupLayout->addWidget(backupInfoLabel);

    // Backup status label
    QSettings settings;
    QString lastBackupTime = settings.value("autoBackup/lastBackupTime", "").toString();
    bool lastBackupSuccess = settings.value("autoBackup/lastBackupStatus", true).toBool();

    QLabel* statusLabel = new QLabel();
    if (!lastBackupTime.isEmpty()) {
        QDateTime backupDateTime = QDateTime::fromString(lastBackupTime, Qt::ISODate);
        QString timeAgo = backupDateTime.toString("yyyy-MM-dd hh:mm:ss");

        if (lastBackupSuccess) {
            statusLabel->setText(QString("Last automatic backup: %1").arg(timeAgo));
            statusLabel->setStyleSheet("color: green; font-size: 11px;");
        } else {
            QString error = settings.value("autoBackup/lastBackupError", "Unknown error").toString();
            statusLabel->setText(QString("⚠️ Last backup failed (%1): %2").arg(timeAgo).arg(error));
            statusLabel->setStyleSheet("color: red; font-size: 11px;");
        }
    } else {
        statusLabel->setText("No automatic backups yet");
        statusLabel->setStyleSheet("color: gray; font-size: 11px;");
    }
    statusLabel->setWordWrap(true);
    backupGroupLayout->addWidget(statusLabel);

    generalLayout->addWidget(backupGroup);
    loadBackupSettings();

    generalLayout->addStretch();

    tabWidget->addTab(generalTab, "General");

    // Create Look tab
    QWidget* lookTab = new QWidget();
    QVBoxLayout* lookLayout = new QVBoxLayout(lookTab);
    lookLayout->setContentsMargins(Constants::Margins::SMALL, Constants::Margins::SMALL, Constants::Margins::SMALL, Constants::Margins::SMALL);
    lookLayout->setSpacing(Constants::Spacing::SMALL);

    // Highlighting colors group
    QGroupBox* highlightGroup = new QGroupBox("Highlighting Colors");
    QVBoxLayout* highlightLayout = new QVBoxLayout(highlightGroup);
    highlightLayout->setSpacing(8);

    // Define highlight levels
    QStringList levels = {"Recognized", "Learning", "Known", "WellKnown", "Unknown", "Ignored"};
    QStringList levelLabels = {"New:", "Learning:", "Known:", "Well Known:", "Unknown:", "Ignored:"};

    loadColors();

    // Create header row
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(Constants::Spacing::LARGE);
    QLabel* emptyLabel = new QLabel();
    emptyLabel->setMinimumWidth(Constants::Control::SETTINGS_LABEL_MIN_WIDTH);
    headerLayout->addWidget(emptyLabel);

    QLabel* lightLabel = new QLabel("Light Theme");
    lightLabel->setAlignment(Qt::AlignCenter);
    lightLabel->setMinimumWidth(Constants::Control::SETTINGS_BUTTON_MIN_WIDTH);
    lightLabel->setMaximumWidth(Constants::Control::SETTINGS_BUTTON_MAX_WIDTH);
    QFont headerFont = lightLabel->font();
    headerFont.setBold(true);
    lightLabel->setFont(headerFont);
    headerLayout->addWidget(lightLabel);

    QLabel* darkLabel = new QLabel("Dark Theme");
    darkLabel->setAlignment(Qt::AlignCenter);
    darkLabel->setMinimumWidth(Constants::Control::SETTINGS_BUTTON_MIN_WIDTH);
    darkLabel->setMaximumWidth(Constants::Control::SETTINGS_BUTTON_MAX_WIDTH);
    darkLabel->setFont(headerFont);
    headerLayout->addWidget(darkLabel);

    headerLayout->addStretch();
    highlightLayout->addLayout(headerLayout);

    // Create color picker for each level (light and dark)
    for (int i = 0; i < levels.size(); ++i) {
        QHBoxLayout* rowLayout = new QHBoxLayout();
        rowLayout->setSpacing(Constants::Spacing::LARGE);

        QLabel* label = new QLabel(levelLabels[i]);
        label->setMinimumWidth(Constants::Control::SETTINGS_LABEL_MIN_WIDTH);
        rowLayout->addWidget(label);

        // Light theme color button
        QPushButton* lightButton = new QPushButton();
        lightButton->setMinimumSize(Constants::Control::SETTINGS_BUTTON_MIN_WIDTH, Constants::Control::MIN_BUTTON_HEIGHT);
        lightButton->setMaximumWidth(Constants::Control::SETTINGS_BUTTON_MAX_WIDTH);
        lightButton->setProperty("level", levels[i]);
        lightButton->setProperty("theme", "light");
        updateButtonColor(lightButton, lightColors[levels[i]]);
        connect(lightButton, &QPushButton::clicked, this, &SettingsDialog::onColorButtonClicked);
        lightColorButtons[levels[i]] = lightButton;
        rowLayout->addWidget(lightButton);

        // Dark theme color button
        QPushButton* darkButton = new QPushButton();
        darkButton->setMinimumSize(Constants::Control::SETTINGS_BUTTON_MIN_WIDTH, Constants::Control::MIN_BUTTON_HEIGHT);
        darkButton->setMaximumWidth(Constants::Control::SETTINGS_BUTTON_MAX_WIDTH);
        darkButton->setProperty("level", levels[i]);
        darkButton->setProperty("theme", "dark");
        updateButtonColor(darkButton, darkColors[levels[i]]);
        connect(darkButton, &QPushButton::clicked, this, &SettingsDialog::onColorButtonClicked);
        darkColorButtons[levels[i]] = darkButton;
        rowLayout->addWidget(darkButton);

        rowLayout->addStretch();

        highlightLayout->addLayout(rowLayout);
    }

    // Add Reset to Defaults button
    QHBoxLayout* resetLayout = new QHBoxLayout();
    resetLayout->addStretch();

    QPushButton* resetButton = new QPushButton("Reset All to Defaults");
    resetButton->setMinimumHeight(Constants::Control::MIN_BUTTON_HEIGHT);
    resetButton->setCursor(Qt::PointingHandCursor);
    resetButton->setStyleSheet(QString(R"(
        QPushButton {
            background-color: transparent;
            color: %1;
            border: 1px solid %1;
            border-radius: 4px;
            padding: 6px 16px;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: %1;
            color: white;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::Primary)));
    connect(resetButton, &QPushButton::clicked, this, &SettingsDialog::onResetColorsClicked);
    resetLayout->addWidget(resetButton);

    highlightLayout->addLayout(resetLayout);

    lookLayout->addWidget(highlightGroup);

    // Font size group
    QGroupBox* fontGroup = new QGroupBox("Font");
    QHBoxLayout* fontLayout = new QHBoxLayout(fontGroup);

    QLabel* fontSizeLabel = new QLabel("Font Size:");
    fontLayout->addWidget(fontSizeLabel);

    fontSizeSpinBox = new QSpinBox();
    fontSizeSpinBox->setMinimum(8);
    fontSizeSpinBox->setMaximum(Constants::Font::MAX_SIZE);
    fontSizeSpinBox->setSuffix(" pt");
    fontSizeSpinBox->setMinimumWidth(Constants::Control::SPIN_BOX_MIN_WIDTH);
    loadFontSize();
    fontLayout->addWidget(fontSizeSpinBox);

    fontLayout->addStretch();

    lookLayout->addWidget(fontGroup);
    lookLayout->addStretch();

    tabWidget->addTab(lookTab, "Look");

    // Add dialog buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::onReject);
    mainLayout->addWidget(buttonBox);
}

SettingsDialog::~SettingsDialog()
{
}

void SettingsDialog::loadColors()
{
    QSettings settings;

    QStringList levels = {"Recognized", "Learning", "Known", "WellKnown", "Unknown", "Ignored"};
    for (const QString& level : levels) {
        // Load light theme colors
        QColor defaultLightColor = getDefaultLightColor(level);
        QColor lightColor = settings.value("colors/light/" + level, defaultLightColor).value<QColor>();
        lightColors[level] = lightColor;

        // Load dark theme colors
        QColor defaultDarkColor = getDefaultDarkColor(level);
        QColor darkColor = settings.value("colors/dark/" + level, defaultDarkColor).value<QColor>();
        darkColors[level] = darkColor;
    }
}

void SettingsDialog::saveColors()
{
    QSettings settings;

    // Save light theme colors
    for (auto it = lightColors.begin(); it != lightColors.end(); ++it) {
        settings.setValue("colors/light/" + it.key(), it.value());
    }

    // Save dark theme colors
    for (auto it = darkColors.begin(); it != darkColors.end(); ++it) {
        settings.setValue("colors/dark/" + it.key(), it.value());
    }
}

void SettingsDialog::updateButtonColor(QPushButton* button, const QColor& color)
{
    if (color == Qt::transparent || !color.isValid()) {
        // Use background color for transparent (no highlight)
        QString bgColor = ThemeManager::instance().getColorHex(ThemeColor::CardBackground);
        button->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; }")
                             .arg(bgColor)
                             .arg(ThemeManager::instance().getColorHex(ThemeColor::TextPrimary)));
        button->setText("TRANSPARENT");
    } else {
        QString colorName = color.name();
        // Determine text color based on brightness
        int brightness = (color.red() * Constants::Color::BRIGHTNESS_RED_WEIGHT + color.green() * Constants::Color::BRIGHTNESS_GREEN_WEIGHT + color.blue() * Constants::Color::BRIGHTNESS_BLUE_WEIGHT) / Constants::Color::BRIGHTNESS_DIVISOR;
        QString textColor = brightness > Constants::Color::BRIGHTNESS_THRESHOLD ? "black" : "white";

        button->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; }")
                             .arg(colorName).arg(textColor));
        button->setText(colorName.toUpper());
    }
}

QColor SettingsDialog::getDefaultLightColor(const QString& level)
{
    // Get colors from ThemeManager
    ThemeMode savedMode = ThemeManager::instance().getThemeMode();
    ThemeManager::instance().setThemeMode(ThemeMode::Light);
    QColor color;

    if (level == "Recognized") color = ThemeManager::instance().getColor(ThemeColor::LevelRecognized);
    else if (level == "Learning") color = ThemeManager::instance().getColor(ThemeColor::LevelLearning);
    else if (level == "Known") color = ThemeManager::instance().getColor(ThemeColor::LevelKnown);
    else if (level == "WellKnown") color = ThemeManager::instance().getColor(ThemeColor::LevelWellKnown);
    else if (level == "Unknown") color = ThemeManager::instance().getColor(ThemeColor::LevelUnknown);
    else if (level == "Ignored") color = ThemeManager::instance().getColor(ThemeColor::LevelIgnored);
    else color = QColor(Qt::transparent);

    ThemeManager::instance().setThemeMode(savedMode);
    return color;
}

QColor SettingsDialog::getDefaultDarkColor(const QString& level)
{
    // Get colors from ThemeManager
    ThemeMode savedMode = ThemeManager::instance().getThemeMode();
    ThemeManager::instance().setThemeMode(ThemeMode::Dark);
    QColor color;

    if (level == "Recognized") color = ThemeManager::instance().getColor(ThemeColor::LevelRecognized);
    else if (level == "Learning") color = ThemeManager::instance().getColor(ThemeColor::LevelLearning);
    else if (level == "Known") color = ThemeManager::instance().getColor(ThemeColor::LevelKnown);
    else if (level == "WellKnown") color = ThemeManager::instance().getColor(ThemeColor::LevelWellKnown);
    else if (level == "Unknown") color = ThemeManager::instance().getColor(ThemeColor::LevelUnknown);
    else if (level == "Ignored") color = ThemeManager::instance().getColor(ThemeColor::LevelIgnored);
    else color = QColor(Qt::transparent);

    ThemeManager::instance().setThemeMode(savedMode);
    return color;
}

void SettingsDialog::onColorButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;

    QString level = button->property("level").toString();
    QString theme = button->property("theme").toString();

    QColor currentColor = (theme == "light") ? lightColors[level] : darkColors[level];

    QString title = QString("Choose Color - %1 (%2 Theme)")
                        .arg(level)
                        .arg(theme == "light" ? "Light" : "Dark");

    QColor newColor = QColorDialog::getColor(currentColor, this, title);
    if (newColor.isValid()) {
        if (theme == "light") {
            lightColors[level] = newColor;
        } else {
            darkColors[level] = newColor;
        }
        updateButtonColor(button, newColor);
    }
}

void SettingsDialog::onResetColorsClicked()
{
    QStringList levels = {"Recognized", "Learning", "Known", "WellKnown", "Unknown", "Ignored"};

    // Reset all colors to defaults
    for (const QString& level : levels) {
        lightColors[level] = getDefaultLightColor(level);
        darkColors[level] = getDefaultDarkColor(level);

        // Update button displays
        if (lightColorButtons.contains(level)) {
            updateButtonColor(lightColorButtons[level], lightColors[level]);
        }
        if (darkColorButtons.contains(level)) {
            updateButtonColor(darkColorButtons[level], darkColors[level]);
        }
    }
}

void SettingsDialog::loadFontSize()
{
    QSettings settings;
    int fontSize = settings.value("font/size", Constants::Font::SIZE_DEFAULT).toInt();
    fontSizeSpinBox->setValue(fontSize);
}

void SettingsDialog::saveFontSize()
{
    QSettings settings;
    settings.setValue("font/size", fontSizeSpinBox->value());
}

void SettingsDialog::loadTheme()
{
    QString themeMode = ThemeManager::instance().getThemeMode() == ThemeMode::Light ? "light" :
                        ThemeManager::instance().getThemeMode() == ThemeMode::Dark ? "dark" : "auto";

    int index = themeComboBox->findData(themeMode);
    if (index >= 0) {
        themeComboBox->setCurrentIndex(index);
    }
}

void SettingsDialog::saveTheme()
{
    QString modeStr = themeComboBox->currentData().toString();
    ThemeMode mode = modeStr == "light" ? ThemeMode::Light :
                     modeStr == "dark" ? ThemeMode::Dark : ThemeMode::Auto;

    ThemeManager::instance().setThemeMode(mode);
}

void SettingsDialog::onAccept()
{
    saveColors();
    saveFontSize();
    saveTheme();
    saveBackupSettings();
    accept();
}

void SettingsDialog::loadBackupSettings()
{
    QSettings settings;
    bool enabled = settings.value("autoBackup/enabled", true).toBool();
    int maxBackups = settings.value("autoBackup/maxBackups", 5).toInt();

    autoBackupCheckBox->setChecked(enabled);
    maxBackupsSpinBox->setValue(maxBackups);
}

void SettingsDialog::saveBackupSettings()
{
    QSettings settings;
    settings.setValue("autoBackup/enabled", autoBackupCheckBox->isChecked());
    settings.setValue("autoBackup/maxBackups", maxBackupsSpinBox->value());
}

void SettingsDialog::onReject()
{
    reject();
}
