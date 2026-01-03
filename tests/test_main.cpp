#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QTemporaryDir>
#include "kotodama/databasemanager.h"

// Custom main for Qt-based tests
// This sets up QCoreApplication and initializes a temporary test database

QString g_testDbPath;

int main(int argc, char **argv)
{
    // Initialize Qt application
    QCoreApplication app(argc, argv);
    app.setOrganizationName("KotodamaTests");
    app.setApplicationName("kotodama-tests");

    qDebug() << "Setting up test database...";

    // Create a temporary directory for the test database
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString testDataPath = tempPath + "/kotodama-test-" + QString::number(QCoreApplication::applicationPid());
    QDir dir;
    dir.mkpath(testDataPath);

    g_testDbPath = testDataPath + "/test-database.sqlite";

    qDebug() << "Test database location:" << g_testDbPath;

    // Override the AppDataLocation for DatabaseManager
    // We need to ensure DatabaseManager uses our test database
    QStandardPaths::setTestModeEnabled(true);

    // Initialize database
    DatabaseManager::instance().initialize();

    qDebug() << "Test database initialized successfully";

    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // Run all tests
    int result = RUN_ALL_TESTS();

    qDebug() << "Tests completed. Cleaning up...";

    // Close database connection
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);

    // Delete test database file
    if (QFile::exists(g_testDbPath)) {
        if (QFile::remove(g_testDbPath)) {
            qDebug() << "Test database deleted:" << g_testDbPath;
        } else {
            qWarning() << "Failed to delete test database:" << g_testDbPath;
        }
    }

    // Clean up test directory
    dir.rmpath(testDataPath);

    qDebug() << "Cleanup completed";

    return result;
}
