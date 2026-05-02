#include "kotodama/mainwindow.h"
#include "kotodama/importvocabdialog.h"
#include "kotodama/languagemanagerdialog.h"
#include "kotodama/languageselectiondialog.h"
#include "kotodama/settingsdialog.h"
#include "kotodama/languagemanager.h"
#include "kotodama/languagemodulemanager.h"
#include "kotodama/termmanager.h"
#include "kotodama/thememanager.h"
#include "kotodama/databasemanager.h"
#include "kotodama/backupmanager.h"
#include "kotodama/constants.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <QtWidgets/qstatusbar.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , viewer(nullptr)
    , viewStack(nullptr)
    , firstRunWidget(nullptr)
    , normalWidget(nullptr)
    , languageSelector(nullptr)
    , continueButton(nullptr)
    , languageComboBox(nullptr)
    , importButton(nullptr)
    , scrollArea(nullptr)
    , scrollContent(nullptr)
    , cardLayout(nullptr)
{
    // Window setup
#ifdef DEV_BUILD
    setWindowTitle("kotodama-dev");
#else
    setWindowTitle("kotodama");
#endif
    resize(Constants::Window::DEFAULT_WIDTH, Constants::Window::DEFAULT_HEIGHT);
    setMinimumSize(Constants::Window::MIN_WIDTH, Constants::Window::MIN_HEIGHT);

    setStyleSheet(QString("QMainWindow { background-color: %1; }")
                      .arg(ThemeManager::instance().getColorHex(ThemeColor::WindowBackground)));

    // Create stacked widget for first-run vs normal UI
    viewStack = new QStackedWidget(this);
    setCentralWidget(viewStack);

    // Check if user has selected languages
    if (!DatabaseManager::instance().hasUserLanguages())
    {
        setupFirstRunUI();
        viewStack->setCurrentWidget(firstRunWidget);
    }
    else
    {
        setupNormalUI();
        viewStack->setCurrentWidget(normalWidget);
    }

    // Check if last automatic backup failed and warn user
    QSettings settings;
    QString lastBackupTime = settings.value("autoBackup/lastBackupTime", "").toString();
    bool lastBackupSuccess = settings.value("autoBackup/lastBackupStatus", true).toBool();
    bool backupWarningShown = settings.value("autoBackup/warningShown", false).toBool();

    if (!lastBackupTime.isEmpty() && !lastBackupSuccess && !backupWarningShown) {
        QDateTime backupDateTime = QDateTime::fromString(lastBackupTime, Qt::ISODate);
        QString error = settings.value("autoBackup/lastBackupError", "Unknown error").toString();

        QTimer::singleShot(500, this, [this, backupDateTime, error]() {
            QMessageBox::warning(
                this,
                "Backup Failed",
                QString("The last automatic backup failed on %1.\n\n"
                       "Error: %2\n\n"
                       "You may want to create a manual backup (File → Create Backup) "
                       "to ensure your data is safe.")
                    .arg(backupDateTime.toString("yyyy-MM-dd hh:mm:ss"))
                    .arg(error)
            );
        });

        // Mark warning as shown so we don't show it again until next failure
        settings.setValue("autoBackup/warningShown", true);
    }

    // Check database integrity on startup
    IntegrityCheckResult integrityResult = DatabaseManager::instance().checkIntegrity();
    if (!integrityResult.ok) {
        QString errorDetails = integrityResult.errors.join("\n");

        QTimer::singleShot(1000, this, [this, errorDetails]() {
            QMessageBox::critical(
                this,
                "Database Corruption Detected",
                QString("The application database appears to be corrupted:\n\n%1\n\n"
                       "Your data may be at risk. It is strongly recommended to restore "
                       "from a backup (File → Restore from Backup).\n\n"
                       "If you continue using the application, you may experience errors "
                       "or data loss.")
                    .arg(errorDetails)
            );
        });
    }

    //  Warning flag is reset on successful backup
}

MainWindow::~MainWindow()
{
    // Qt parent-child ownership handles cleanup
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Check if automatic backups are enabled
    QSettings settings;
    bool autoBackupEnabled = settings.value("autoBackup/enabled", true).toBool();
    int maxBackups = settings.value("autoBackup/maxBackups", 5).toInt();

    if (autoBackupEnabled) {
        // Create automatic backup
        ExportResult result = BackupManager::instance().createAutomaticBackup();

        if (result.success) {
            qDebug() << "Automatic backup created on exit:" << result.filePath;

            // Rotate old backups
            BackupManager::instance().rotateBackups(maxBackups);
        } else {
            qDebug() << "Failed to create automatic backup:" << result.errorMessage;
            // Don't block closing even if backup fails
        }
    }

    // Call base class implementation
    QMainWindow::closeEvent(event);
}

void MainWindow::loadTexts()
{
    // Clear existing cards
    qDeleteAll(textCards);
    textCards.clear();

    QString languageCode = getCurrentLanguage();

    // Clear all widgets from the layout (including empty state label and stretch)
    QLayoutItem* item;
    while ((item = cardLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    // If the current language requires a support module that isn't
    // installed, replace the card view with an install-prompt banner.
    // The dropdown remains usable so the user can switch languages.
    auto& langMgr = LanguageManager::instance();
    bool moduleMissing =
        !languageCode.isEmpty()
        && LanguageManager::languageRequiresModule(languageCode)
        && !LanguageModuleManager::instance().isInstalled(languageCode);

    if (moduleMissing) {
        QString langName = langMgr.getLanguageByCode(languageCode).name();
        if (langName.isEmpty()) langName = languageCode;

        QWidget* banner = new QWidget();
        QVBoxLayout* bannerLayout = new QVBoxLayout(banner);
        bannerLayout->setAlignment(Qt::AlignCenter);
        bannerLayout->setSpacing(Constants::Spacing::MEDIUM);

        QLabel* title = new QLabel(QString("%1 support is not installed").arg(langName));
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(QString("color: %1; font-size: 16pt; font-weight: 600;")
                                 .arg(ThemeManager::instance().getColorHex(ThemeColor::TextPrimary)));

        QLabel* desc = new QLabel(
            QString("Install the %1 support module to import or read texts in this language.")
                .arg(langName));
        desc->setAlignment(Qt::AlignCenter);
        desc->setWordWrap(true);
        desc->setStyleSheet(QString("color: %1; font-size: 12pt;")
                                 .arg(ThemeManager::instance().getColorHex(ThemeColor::TextSecondary)));

        QPushButton* installBtn = new QPushButton("Install support module");
        installBtn->setCursor(Qt::PointingHandCursor);
        installBtn->setMinimumHeight(Constants::Control::IMPORT_BUTTON_MIN_HEIGHT);
        connect(installBtn, &QPushButton::clicked, this, [this, languageCode]() {
            LanguageManagerDialog dialog(this);
            dialog.exec();
            // Refresh: if the user installed the module, the banner
            // gives way to the normal card view.
            loadTexts();
        });

        bannerLayout->addStretch();
        bannerLayout->addWidget(title);
        bannerLayout->addWidget(desc);
        QHBoxLayout* btnRow = new QHBoxLayout();
        btnRow->addStretch();
        btnRow->addWidget(installBtn);
        btnRow->addStretch();
        bannerLayout->addLayout(btnRow);
        bannerLayout->addStretch();

        cardLayout->addWidget(banner);
        cardLayout->addStretch();

        if (importButton) importButton->setEnabled(false);
        return;
    }

    if (importButton) importButton->setEnabled(true);

    // Get texts from model
    QList<TextDisplayItem> texts = model.getTexts(languageCode);

    if (texts.isEmpty()) {
        // Show empty state message
        QLabel* emptyLabel = new QLabel("No texts imported yet");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet(QString("color: %1; font-size: 14pt; padding: 40px;")
                                      .arg(ThemeManager::instance().getColorHex(ThemeColor::TextSecondary)));
        cardLayout->addWidget(emptyLabel);
    } else {
        // Create a card for each text
        for (const TextDisplayItem& text : texts) {
            // Calculate progress stats
            TextProgressStats stats = model.calculateTextProgress(text.uuid);

            // Create card
            TextCard* card = new TextCard(scrollContent);
            card->setTextInfo(text, stats);

            // Connect signals
            connect(card, &TextCard::clicked, this, &MainWindow::onCardClicked);
            connect(card, &TextCard::deleteRequested, this, &MainWindow::onDeleteRequested);

            // Add to layout and track
            cardLayout->addWidget(card);
            textCards.append(card);
        }
    }

    // Add stretch at the end
    cardLayout->addStretch();

    // Update status bar
    QString statusMsg = QString("%1 text(s)").arg(texts.size());
    if (!languageCode.isEmpty()) {
        QString langName = languageComboBox->currentText();
        statusMsg += QString(" in %1").arg(langName);
    }
    statusBar()->showMessage(statusMsg);
}

void MainWindow::onLanguageChanged(int index)
{
    Q_UNUSED(index);

    // Save the selected language
    saveLanguage();

    loadTexts();

    // Pre-load trie for new language
    QString lang = getCurrentLanguage();
    if (!lang.isEmpty()) {
        TermManager::instance().getTrieForLanguage(lang);
    }
}

QString MainWindow::getCurrentLanguage()
{
    return languageComboBox->currentData().toString();
}

void MainWindow::onImportClicked()
{
    // Step 1: Select file
    QString sourceFile = QFileDialog::getOpenFileName(
        this,
        "Select Text File to Import",
        QDir::homePath(),
        "Text Files (*.txt);;All Files (*)"
    );

    if (sourceFile.isEmpty()) {
        return; // User cancelled
    }

    // Step 2: Ask for title
    QFileInfo fileInfo(sourceFile);
    QString defaultTitle = fileInfo.baseName();

    bool ok;
    QString title = QInputDialog::getText(
        this,
        "Import Text",
        "Enter a title for this text:",
        QLineEdit::Normal,
        defaultTitle,
        &ok
    );

    if (!ok || title.isEmpty()) {
        return;
    }

    QString languageCode = getCurrentLanguage();

    // Step 4: Import using model
    ImportTextResult result = model.importText(sourceFile, title, languageCode);

    if (!result.success) {
        QMessageBox::warning(this, "Import Failed",
                            "Failed to import the text file.\n" + result.errorMessage);
        return;
    }

    // Step 5: Refresh the list
    loadTexts();

    QMessageBox::information(this, "Success", "Text imported successfully!");
}

void MainWindow::onCardClicked(QString uuid)
{
    // Get text info from model
    TextDisplayItem textInfo = model.getTextByUuid(uuid);

    // Get file path from model
    QString filePath = model.getTextFilePath(uuid);

    // Create or reuse viewer
    if (!viewer) {
        viewer = new EbookViewer(this);
    }

    viewer->loadFile(filePath, uuid, textInfo.language, textInfo.title);
    viewer->show();
    viewer->raise();

    // Update last opened time via model
    model.updateLastOpened(uuid);

    // Refresh the card list to re-sort by most recently opened
    // Use QTimer to defer until after the click event completes
    QTimer::singleShot(0, this, &MainWindow::loadTexts);
}

void MainWindow::onDeleteRequested(QString uuid)
{
    // Get text info for confirmation dialog
    TextDisplayItem textInfo = model.getTextByUuid(uuid);

    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Delete Text",
        QString("Are you sure you want to delete \"%1\"?\n\nThis will permanently delete the text file.").arg(textInfo.title),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        // Delete the text
        bool success = model.deleteText(uuid);

        if (success) {
            // Refresh the card list
            loadTexts();
            statusBar()->showMessage("Text deleted successfully", Constants::Timing::STATUS_MESSAGE_DURATION_MS);
        } else {
            QMessageBox::warning(this, "Delete Failed", "Failed to delete the text.");
        }
    }
}

void MainWindow::onImportVocabClicked()
{
    ImportVocabDialog dialog(this);

    if (dialog.exec() == QDialog::Accepted) {
        QString language = dialog.getSelectedLanguage();
        QList<ImportVocabDialog::ImportData> data = dialog.getImportData();

        // Convert to model format and check for duplicates
        QList<MainWindowModel::VocabImportItem> items;
        QStringList duplicates;
        for (const auto& importData : data) {
            MainWindowModel::VocabImportItem item;
            item.term = importData.term;
            item.language = language;
            item.level = static_cast<TermLevel>(importData.level.toInt());
            item.definition = importData.definition;
            item.pronunciation = importData.pronunciation;
            items.append(item);

            // Check if term already exists
            if (TermManager::instance().termExists(item.term, language)) {
                duplicates.append(item.term);
            }
        }

        // If duplicates exist, ask user what to do
        bool updateDuplicates = false;
        if (!duplicates.isEmpty()) {
            QString dupMessage = QString("Found %1 existing term(s):\n\n%2\n\n"
                                        "Do you want to update them with the new data?\n\n"
                                        "Yes - Update existing terms\n"
                                        "No - Skip existing terms\n"
                                        "Cancel - Abort import")
                                    .arg(duplicates.size())
                                    .arg(duplicates.size() <= Constants::Batch::MAX_DUPLICATE_DISPLAY
                                         ? duplicates.join(", ")
                                         : duplicates.mid(0, Constants::Batch::MAX_DUPLICATE_DISPLAY).join(", ") +
                                           QString("... and %1 more").arg(duplicates.size() - Constants::Batch::MAX_DUPLICATE_DISPLAY));

            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                "Duplicate Terms Found",
                dupMessage,
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                QMessageBox::No
            );

            if (reply == QMessageBox::Cancel) {
                return; // User cancelled import
            }

            updateDuplicates = (reply == QMessageBox::Yes);
        }

        // Import using model (with update flag for duplicates)
        MainWindowModel::VocabImportResult result = model.importVocabulary(items, updateDuplicates);

        // Display result
        QString message = QString("Import complete!\n\nSuccess: %1\nFailed: %2")
                              .arg(result.successCount).arg(result.failCount);

        if (!result.errors.isEmpty() && result.errors.size() <= Constants::Batch::MAX_ERROR_DISPLAY) {
            message += "\n\nErrors:\n" + result.errors.join("\n");
        } else if (!result.errors.isEmpty()) {
            message += QString("\n\n%1 errors occurred (too many to display)")
                          .arg(result.errors.size());
        }

        QMessageBox::information(this, "Import Complete", message);

        // Invalidate progress cache for this language
        model.invalidateProgressCache(language);

        // Refresh the text list
        loadTexts();
    }
}

void MainWindow::onSettingsClicked()
{
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::onManageLanguagesClicked()
{
    LanguageManagerDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // Refresh language combo box in case languages were added/removed
        QString currentLang = getCurrentLanguage();

        populateLanguageComboBox();

        // Restore previous selection if it still exists
        int index = languageComboBox->findData(currentLang);
        if (index >= 0) {
            languageComboBox->setCurrentIndex(index);
        }
    }
}

void MainWindow::saveLanguage()
{
    QSettings settings;
    QString languageCode = getCurrentLanguage();
    settings.setValue("language/selected", languageCode);
}

void MainWindow::loadLanguage()
{
    QSettings settings;
    QString savedLanguage = settings.value("language/selected", "").toString();

    if (!savedLanguage.isEmpty()) {
        // Find and set the saved language in the combo box
        int index = languageComboBox->findData(savedLanguage);
        if (index >= 0) {
            languageComboBox->setCurrentIndex(index);
        }
    }
}

void MainWindow::activateWindow()
{
    // Bring window to front
    show();
    raise();
    QMainWindow::activateWindow();

#ifdef Q_OS_MACOS
    // Request attention if minimized
    QApplication::alert(this);
#endif

#ifdef Q_OS_WIN
    // Flash taskbar on Windows
    QApplication::alert(this, 0);
#endif
}

void MainWindow::setupFirstRunUI()
{
    firstRunWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(firstRunWidget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(Constants::Margins::LARGE * 3, Constants::Margins::LARGE * 2,
                               Constants::Margins::LARGE * 3, Constants::Margins::LARGE * 2);

    // Welcome message
    QLabel* welcomeLabel = new QLabel("Welcome to kotodama!");
    QFont welcomeFont = welcomeLabel->font();
    welcomeFont.setPointSize(Constants::Font::SIZE_LARGE * 2);
    welcomeFont.setBold(true);
    welcomeLabel->setFont(welcomeFont);
    welcomeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(welcomeLabel);

    QLabel* subtitleLabel = new QLabel("To get started, select the languages you want to learn:");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    QFont subtitleFont = subtitleLabel->font();
    subtitleFont.setPointSize(Constants::Font::SIZE_MEDIUM);
    subtitleLabel->setFont(subtitleFont);
    subtitleLabel->setStyleSheet(QString("color: %1; margin: 10px 0;")
        .arg(ThemeManager::instance().getColorHex(ThemeColor::TextSecondary)));
    layout->addWidget(subtitleLabel);

    // Language selection widget
    languageSelector = new LanguageSelectionWidget();
    languageSelector->setMaximumWidth(600);
    layout->addWidget(languageSelector, 1);

    // Continue button
    continueButton = new QPushButton("Continue");
    continueButton->setMinimumWidth(Constants::Control::IMPORT_BUTTON_MIN_WIDTH);
    continueButton->setMinimumHeight(Constants::Control::IMPORT_BUTTON_MIN_HEIGHT);
    continueButton->setEnabled(false);
    continueButton->setCursor(Qt::PointingHandCursor);

    QString buttonTextColor = ThemeManager::instance().isDarkMode() ? "#000000" : "#FFFFFF";
    continueButton->setStyleSheet(QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: none;
            border-radius: 4px;
            padding: 10px 24px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover:enabled {
            background-color: %3;
        }
        QPushButton:pressed:enabled {
            background-color: %4;
        }
        QPushButton:disabled {
            background-color: %5;
            color: %6;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::Primary))
       .arg(buttonTextColor)
       .arg(ThemeManager::instance().getColorHex(ThemeColor::PrimaryHover))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::PrimaryPressed))
       .arg(ThemeManager::instance().isDarkMode() ? "#2A2A2A" : "#CCCCCC")
       .arg(ThemeManager::instance().isDarkMode() ? "#666666" : "#999999"));

    connect(continueButton, &QPushButton::clicked, this, &MainWindow::onFirstRunContinue);
    connect(languageSelector, &LanguageSelectionWidget::selectionChanged,
            this, [this]() { continueButton->setEnabled(languageSelector->hasSelection()); });

    layout->addWidget(continueButton, 0, Qt::AlignCenter);

    viewStack->addWidget(firstRunWidget);
}

void MainWindow::setupNormalUI()
{
    normalWidget = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(normalWidget);
    mainLayout->setContentsMargins(Constants::Margins::SMALL, Constants::Margins::SMALL,
                                    Constants::Margins::SMALL, Constants::Margins::SMALL);
    mainLayout->setSpacing(Constants::Spacing::SMALL);

    // Top controls bar
    QWidget* topControlsCard = new QWidget();
    topControlsCard->setStyleSheet(QString(R"(
        QWidget {
            background-color: %1;
            border-radius: 8px;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::CardBackground)));
    topControlsCard->setFixedHeight(Constants::Panel::TOP_CONTROLS_HEIGHT);

    QHBoxLayout* topLayout = new QHBoxLayout(topControlsCard);
    topLayout->setContentsMargins(Constants::Margins::LARGE, Constants::Margins::MEDIUM,
                                   Constants::Margins::LARGE, Constants::Margins::MEDIUM);
    topLayout->setSpacing(Constants::Spacing::EXTRA_LARGE);

    // Language selector section
    QVBoxLayout* languageSectionLayout = new QVBoxLayout();
    languageSectionLayout->setSpacing(4);

    languageComboBox = new QComboBox();
    languageComboBox->setMinimumWidth(Constants::Control::LANGUAGE_COMBO_MIN_WIDTH);
    languageComboBox->setFixedHeight(Constants::Control::LANGUAGE_COMBO_HEIGHT);
    languageComboBox->setCursor(Qt::PointingHandCursor);
    languageComboBox->setStyleSheet(QString(R"(
        QComboBox {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 4px;
            padding: 8px 12px;
            font-size: 14px;
            color: %3;
        }
        QComboBox:hover {
            border: 1px solid %4;
            background-color: %5;
        }
        QComboBox:focus {
            border: 2px solid %4;
            padding: 7px 11px;
        }
        QComboBox::drop-down {
            border: none;
            width: 30px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid %6;
            margin-right: 8px;
        }
        QComboBox QAbstractItemView {
            background-color: %1;
            border: 1px solid %2;
            selection-background-color: %7;
            selection-color: %3;
            padding: 4px;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::InputBackground))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::Border))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::TextPrimary))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::Primary))
       .arg(ThemeManager::instance().isDarkMode() ? "#2A2A2A" : "#F5F5F5")
       .arg(ThemeManager::instance().getColorHex(ThemeColor::TextSecondary))
       .arg(ThemeManager::instance().isDarkMode() ? "#3A4A5A" : "#E3F2FD"));

    // Populate language combo box with user's selected languages
    populateLanguageComboBox();

    // Load saved language selection
    loadLanguage();

    languageSectionLayout->addWidget(languageComboBox);
    topLayout->addLayout(languageSectionLayout);

    topLayout->addStretch();

    // Import button
    importButton = new QPushButton("Import Text");
    importButton->setMinimumWidth(Constants::Control::IMPORT_BUTTON_MIN_WIDTH);
    importButton->setMinimumHeight(Constants::Control::IMPORT_BUTTON_MIN_HEIGHT);
    importButton->setCursor(Qt::PointingHandCursor);
    QString importTextColor = ThemeManager::instance().isDarkMode() ? "#000000" : "#FFFFFF";
    importButton->setStyleSheet(QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: none;
            border-radius: 4px;
            padding: 10px 24px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: %3;
        }
        QPushButton:pressed {
            background-color: %4;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::Primary))
       .arg(importTextColor)
       .arg(ThemeManager::instance().getColorHex(ThemeColor::PrimaryHover))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::PrimaryPressed)));
    topLayout->addWidget(importButton);

    mainLayout->addWidget(topControlsCard);

    // Scroll area for cards
    scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(QString("QScrollArea { background-color: %1; border: none; }")
                                  .arg(ThemeManager::instance().getColorHex(ThemeColor::WindowBackground)));

    // Scroll content widget
    scrollContent = new QWidget();
    scrollContent->setStyleSheet(QString("background-color: %1;")
                                     .arg(ThemeManager::instance().getColorHex(ThemeColor::WindowBackground)));
    cardLayout = new QVBoxLayout(scrollContent);
    cardLayout->setContentsMargins(Constants::Margins::LARGE, Constants::Margins::LARGE,
                                    Constants::Margins::LARGE, Constants::Margins::LARGE);
    cardLayout->setSpacing(Constants::Spacing::MEDIUM);
    cardLayout->addStretch();

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);

    viewStack->addWidget(normalWidget);

    // Menu bar
    QMenuBar* menuBar = new QMenuBar(this);

    // Application menu (macOS style)
    QMenu* appMenu = menuBar->addMenu("Kotodama");
    actionAbout = appMenu->addAction("About Kotodama");
    appMenu->addSeparator();
    actionSettings = appMenu->addAction("Settings...");
    actionSettings->setMenuRole(QAction::PreferencesRole);
    appMenu->addSeparator();
    actionQuit = appMenu->addAction("Quit Kotodama");
    actionQuit->setMenuRole(QAction::QuitRole);
    actionQuit->setShortcut(QKeySequence::Quit);

    // File menu
    QMenu* fileMenu = menuBar->addMenu("&File");
    actionExportData = fileMenu->addAction("Create Backup...");
    actionImportData = fileMenu->addAction("Restore from Backup...");
    fileMenu->addSeparator();
    actionImportVocab = fileMenu->addAction("Import Vocabulary from CSV...");

    // Tools menu
    QMenu* toolsMenu = menuBar->addMenu("&Tools");
    actionManageLanguages = toolsMenu->addAction("Manage Languages...");
    actionManageLearningLanguages = toolsMenu->addAction("Manage Learning Languages...");

    setMenuBar(menuBar);

    // Status bar
    statusBar()->showMessage("Ready");

    // Connect signals
    connect(importButton, &QPushButton::clicked, this, &MainWindow::onImportClicked);
    connect(languageComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLanguageChanged);
    connect(actionAbout, &QAction::triggered, this, &MainWindow::onAboutClicked);
    connect(actionSettings, &QAction::triggered, this, &MainWindow::onSettingsClicked);
    connect(actionQuit, &QAction::triggered, qApp, &QApplication::quit);
    connect(actionExportData, &QAction::triggered, this, &MainWindow::onExportDataClicked);
    connect(actionImportData, &QAction::triggered, this, &MainWindow::onImportDataClicked);
    connect(actionImportVocab, &QAction::triggered, this, &MainWindow::onImportVocabClicked);
    connect(actionManageLanguages, &QAction::triggered, this, &MainWindow::onManageLanguagesClicked);
    connect(actionManageLearningLanguages, &QAction::triggered, this, &MainWindow::onManageLearningLanguagesClicked);

    // Initial load
    loadTexts();

    // Pre-load trie for current language
    QString lang = getCurrentLanguage();
    if (!lang.isEmpty()) {
        TermManager::instance().getTrieForLanguage(lang);
    }
}

void MainWindow::populateLanguageComboBox()
{
    if (!languageComboBox) return;

    languageComboBox->clear();

    QStringList userLanguageCodes = DatabaseManager::instance().getUserLanguages();

    for (const QString& code : userLanguageCodes)
    {
        LanguageConfig config = LanguageManager::instance().getLanguageByCode(code);
        if (!config.code().isEmpty())
        {
            languageComboBox->addItem(config.name(), code);
        }
    }
}

void MainWindow::refreshLanguageList()
{
    QString currentLang = getCurrentLanguage();
    populateLanguageComboBox();

    int index = languageComboBox->findData(currentLang);
    if (index >= 0)
    {
        languageComboBox->setCurrentIndex(index);
    }
    else
    {
        // Current language was removed, switch to first available
        if (languageComboBox->count() > 0)
        {
            languageComboBox->setCurrentIndex(0);
        }
    }
}

void MainWindow::onFirstRunContinue()
{
    QStringList selectedLanguages = languageSelector->getSelectedLanguages();

    if (selectedLanguages.isEmpty())
    {
        QMessageBox::warning(this, "No Languages Selected",
                           "Please select at least one language to continue.");
        return;
    }

    // Save selections to database
    for (const QString& code : selectedLanguages)
    {
        DatabaseManager::instance().addUserLanguage(code);
    }

    // Setup normal UI
    setupNormalUI();

    // Switch to normal view
    viewStack->setCurrentWidget(normalWidget);

    // Show welcome message
    QMessageBox::information(this, "Welcome!",
                           QString("You've selected %1 language(s). You can now import texts to start learning!")
                           .arg(selectedLanguages.size()));
}

void MainWindow::onManageLearningLanguagesClicked()
{
    LanguageSelectionDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        // Refresh language combo box
        refreshLanguageList();

        // Reload texts for current language
        loadTexts();

        statusBar()->showMessage("Learning languages updated",
                                Constants::Timing::STATUS_MESSAGE_DURATION_MS);
    }
}

void MainWindow::onExportDataClicked()
{
    QString defaultFileName = QString("kotodama-backup-%1.kotodama.json")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd"));

    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Data",
        QDir::homePath() + "/" + defaultFileName,
        "Kotodama Backup Files (*.kotodama.json);;All Files (*)"
    );

    if (filePath.isEmpty()) {
        return;  // User cancelled
    }

    // Ensure .kotodama.json extension
    if (!filePath.endsWith(".kotodama.json")) {
        filePath += ".kotodama.json";
    }

    // Perform export
    ExportResult result = BackupManager::instance().exportToFile(filePath);

    if (result.success) {
        QMessageBox::information(
            this,
            "Export Successful",
            QString("Successfully exported data to:\n%1\n\n"
                   "Texts: %2\n"
                   "Terms: %3\n"
                   "File size: %4")
                .arg(filePath)
                .arg(result.textCount)
                .arg(result.termCount)
                .arg(formatFileSize(result.fileSize))
        );
    } else {
        QMessageBox::warning(
            this,
            "Export Failed",
            QString("Failed to export data:\n%1").arg(result.errorMessage)
        );
    }
}

void MainWindow::onImportDataClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Import Data",
        QDir::homePath(),
        "Kotodama Backup Files (*.kotodama.json);;All Files (*)"
    );

    if (filePath.isEmpty()) {
        return;  // User cancelled
    }

    // Validate backup file first
    ValidationResult validation = BackupManager::instance().validateBackupFile(filePath);

    if (!validation.valid) {
        QMessageBox::warning(
            this,
            "Invalid Backup File",
            QString("Cannot import backup:\n%1").arg(validation.errorMessage)
        );
        return;
    }

    // Show confirmation with backup info
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Import Data",
        QString("Import backup from:\n%1\n\n"
               "Backup created: %2\n"
               "Contains:\n"
               "  - %3 texts\n"
               "  - %4 terms\n\n"
               "Existing items will be preserved.\n"
               "Do you want to continue?")
            .arg(QFileInfo(filePath).fileName())
            .arg(validation.version)
            .arg(validation.textCount)
            .arg(validation.termCount),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    // Perform import
    ImportResult result = BackupManager::instance().importFromFile(filePath, true);

    if (result.success) {
        // Refresh UI
        loadTexts();
        refreshLanguageList();

        // Show summary
        QString message = QString(
            "Import completed successfully!\n\n"
            "Texts imported: %1\n"
            "Texts skipped (already exist): %2\n"
            "Terms imported: %3\n"
            "Terms updated: %4"
        )
            .arg(result.textsImported)
            .arg(result.textsSkipped)
            .arg(result.termsImported)
            .arg(result.termsUpdated);

        if (!result.errors.isEmpty()) {
            message += QString("\n\nWarnings (%1):").arg(result.errors.size());
            int showCount = qMin(result.errors.size(), Constants::Batch::MAX_ERROR_DISPLAY);
            for (int i = 0; i < showCount; ++i) {
                message += "\n- " + result.errors[i];
            }
            if (result.errors.size() > showCount) {
                message += QString("\n... and %1 more")
                    .arg(result.errors.size() - showCount);
            }
        }

        QMessageBox::information(this, "Import Complete", message);
    } else {
        QMessageBox::warning(
            this,
            "Import Failed",
            QString("Failed to import data:\n%1").arg(result.errorMessage)
        );
    }
}

QString MainWindow::formatFileSize(qint64 bytes)
{
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    } else {
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    }
}

void MainWindow::onAboutClicked()
{
    QMessageBox::about(
        this,
        "About Kotodama",
        QString("<h3>Kotodama</h3>"
               "<p>Version 0.1</p>"
               "<p>A language learning application for reading and vocabulary acquisition.</p>"
               "<p>Built with Qt %1</p>")
            .arg(QT_VERSION_STR)
    );
}
