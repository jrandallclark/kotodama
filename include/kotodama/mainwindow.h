#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QList>
#include <QStackedWidget>

#include "ebookviewer.h"
#include "mainwindowmodel.h"
#include "textcard.h"
#include "languageselectionwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void activateWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onCardClicked(QString uuid);
    void onTermChangedInViewer(const QString& language);
    void onDeleteRequested(QString uuid);
    void onImportClicked();
    void onLanguageChanged(int index);
    void onAboutClicked();
    void onSettingsClicked();
    void onImportVocabClicked();
    void onExportDataClicked();
    void onImportDataClicked();
    void onManageLanguagesClicked();
    void onManageLearningLanguagesClicked();
    void onFirstRunContinue();

private:
    // Model containing business logic
    MainWindowModel model;

    // Stacked widget for first-run vs normal UI
    QStackedWidget* viewStack;
    QWidget* firstRunWidget;
    QWidget* normalWidget;

    // First-run UI widgets
    LanguageSelectionWidget* languageSelector;
    QPushButton* continueButton;

    // Normal UI widgets
    QComboBox* languageComboBox;
    QPushButton* importButton;
    QScrollArea* scrollArea;
    QWidget* scrollContent;
    QVBoxLayout* cardLayout;
    QList<TextCard*> textCards;
    QAction* actionAbout;
    QAction* actionSettings;
    QAction* actionQuit;
    QAction* actionImportVocab;
    QAction* actionExportData;
    QAction* actionImportData;
    QAction* actionManageLanguages;
    QAction* actionManageLearningLanguages;

    // Other members
    EbookViewer* viewer;

    // UI setup methods
    void setupFirstRunUI();
    void setupNormalUI();

    // UI helper methods
    void loadTexts();
    void populateLanguageComboBox();
    void refreshLanguageList();
    QString getCurrentLanguage();
    void saveLanguage();
    void loadLanguage();
    QString formatFileSize(qint64 bytes);
};
#endif // MAINWINDOW_H
