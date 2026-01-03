#include "kotodama/mainwindow.h"
#include "kotodama/databasemanager.h"
#include "kotodama/librarymanager.h"
#include "kotodama/singleinstanceguard.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QMessageBox>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Set application metadata for QSettings and QLockFile
    // Dev builds use separate data directories to avoid conflicts with release version
#ifdef DEV_BUILD
    a.setOrganizationName("Kotodama-Dev");
    a.setOrganizationDomain("kotodama-dev.app");
    a.setApplicationName("Kotodama-Dev");
    a.setApplicationVersion("0.1-dev");
#else
    a.setOrganizationName("Kotodama");
    a.setOrganizationDomain("kotodama.app");
    a.setApplicationName("Kotodama");
    a.setApplicationVersion("0.1");
#endif

    // Check for existing instance BEFORE database initialization
    // Dev builds use different lock identifier
#ifdef DEV_BUILD
    SingleInstanceGuard guard("kotodama-dev-instance");
#else
    SingleInstanceGuard guard("kotodama-instance");
#endif

    if (guard.isAnotherInstanceRunning()) {
        // Try to activate existing instance
        if (guard.activateExistingInstance()) {
            QMessageBox::information(nullptr, "kotodama Already Running",
                "kotodama is already running. The existing window has been activated.");
        } else {
            QMessageBox::warning(nullptr, "kotodama Already Running",
                "Another instance of kotodama is already running.\n\n"
                "Only one instance can run at a time to prevent database conflicts.");
        }
        return 0; // Exit gracefully
    }

    if (!guard.tryToRun()) {
        QMessageBox::critical(nullptr, "Startup Error",
            "Failed to start kotodama. Another instance may be running.");
        return -1;
    }

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "kotodama_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    if (!DatabaseManager::instance().initialize()) {
        QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QMessageBox::critical(nullptr, "Database Initialization Failed",
                            QString("Failed to initialize the application database.\n\n"
                                   "Expected location: %1\n\n"
                                   "Possible causes:\n"
                                   "• Insufficient disk space\n"
                                   "• Corrupted database file\n\n"
                                   "Solutions:\n"
                                   "• Free up disk space\n"
                                   "• Delete the database file to start fresh\n\n"
                                   "The application will now exit.")
                            .arg(dataPath));
        return -1;
    }

    if (!LibraryManager::instance().initialize()) {
        QString libraryPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QMessageBox::critical(nullptr, "Library Initialization Failed",
                            QString("Failed to initialize the application library.\n\n"
                                   "Expected location: %1\n\n"
                                   "Possible causes:\n"
                                   "• Insufficient disk space\n"
                                   "• No write permissions to the data directory\n\n"
                                   "Solutions:\n"
                                   "• Free up disk space\n"
                                   "• Check folder permissions\n\n"
                                   "The application will now exit.")
                            .arg(libraryPath));
        return -1;
    }

    MainWindow w;

    // Connect activation signal to window
    QObject::connect(&guard, &SingleInstanceGuard::instanceStarted,
                     &w, &MainWindow::activateWindow);

    w.show();
    return a.exec();
}
