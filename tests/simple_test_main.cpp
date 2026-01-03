#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QStandardPaths>

// Simple main for Qt-based tests that don't need database
// This just sets up QCoreApplication and test mode

int main(int argc, char **argv)
{
    // Initialize Qt application
    QCoreApplication app(argc, argv);
    app.setOrganizationName("KotodamaTests");
    app.setApplicationName("kotodama-tests");

    // Enable test mode for isolated paths
    QStandardPaths::setTestModeEnabled(true);

    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // Run all tests
    int result = RUN_ALL_TESTS();

    return result;
}
