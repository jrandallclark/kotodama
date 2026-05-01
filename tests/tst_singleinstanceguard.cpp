#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "kotodama/singleinstanceguard.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QLocalServer>
#include <QEventLoop>
#include <QTimer>

class SingleInstanceGuardTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set application metadata for tests
        QCoreApplication::setOrganizationName("KotodamaTest");
        QCoreApplication::setApplicationName("SingleInstanceTest");
    }

    void TearDown() override {
        // Clean up any lock files and server sockets from tests
        QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QStringList keys = {"test-key", "key1", "key2"};
        for (const QString& key : keys) {
            QString lockPath = tempPath + "/KotodamaTest-SingleInstanceTest-" + key + ".lock";
            QFile::remove(lockPath);

            // Remove server sockets (using simplified server name)
            QLocalServer::removeServer(key);
        }
    }
};

TEST_F(SingleInstanceGuardTest, BasicLockAcquisition) {
    // Arrange & Act
    SingleInstanceGuard guard("test-key");
    bool acquired = guard.tryToRun();

    // Assert
    EXPECT_TRUE(acquired);
    EXPECT_FALSE(guard.isAnotherInstanceRunning());
}

TEST_F(SingleInstanceGuardTest, SecondInstanceDetection) {
    // Arrange
    SingleInstanceGuard guard1("test-key");
    ASSERT_TRUE(guard1.tryToRun());

    // Act
    SingleInstanceGuard guard2("test-key");
    bool running = guard2.isAnotherInstanceRunning();

    // Assert
    EXPECT_TRUE(running);
}

TEST_F(SingleInstanceGuardTest, SecondInstanceCannotAcquireLock) {
    // Arrange
    SingleInstanceGuard guard1("test-key");
    ASSERT_TRUE(guard1.tryToRun());

    // Act
    SingleInstanceGuard guard2("test-key");
    bool acquired = guard2.tryToRun();

    // Assert
    EXPECT_FALSE(acquired);
}

TEST_F(SingleInstanceGuardTest, LockReleaseOnDestruction) {
    // Arrange
    {
        SingleInstanceGuard guard1("test-key");
        ASSERT_TRUE(guard1.tryToRun());
        ASSERT_FALSE(guard1.isAnotherInstanceRunning()); // guard1 holds the lock, no other instance
    } // guard1 destroyed here

    // Act
    SingleInstanceGuard guard2("test-key");
    bool acquired = guard2.tryToRun();

    // Assert
    EXPECT_TRUE(acquired);
    EXPECT_FALSE(guard2.isAnotherInstanceRunning());
}

TEST_F(SingleInstanceGuardTest, CheckBeforeAcquire) {
    // Arrange
    SingleInstanceGuard guard("test-key");

    // Act
    bool running = guard.isAnotherInstanceRunning();

    // Assert - no instance should be running yet
    EXPECT_FALSE(running);
}

TEST_F(SingleInstanceGuardTest, MultipleChecks) {
    // Arrange
    SingleInstanceGuard guard1("test-key");
    ASSERT_TRUE(guard1.tryToRun());

    // Act & Assert
    SingleInstanceGuard guard2("test-key");
    EXPECT_TRUE(guard2.isAnotherInstanceRunning());
    EXPECT_TRUE(guard2.isAnotherInstanceRunning()); // Check multiple times
    EXPECT_TRUE(guard2.isAnotherInstanceRunning());
}

TEST_F(SingleInstanceGuardTest, DifferentKeysIndependent) {
    // Arrange
    SingleInstanceGuard guard1("key1");
    SingleInstanceGuard guard2("key2");

    // Act
    bool acquired1 = guard1.tryToRun();
    bool acquired2 = guard2.tryToRun();

    // Assert - both should succeed with different keys
    EXPECT_TRUE(acquired1);
    EXPECT_TRUE(acquired2);
}

TEST_F(SingleInstanceGuardTest, InstanceStartedSignal) {
#ifdef Q_OS_WIN
    GTEST_SKIP() << "Same-process IPC test hangs on Windows named pipes — "
                    "server's QLocalServer accept routes through the main-thread "
                    "event loop, which is blocked by the activator's synchronous "
                    "waitFor* calls. Tracked in #3.";
#endif
    // Arrange
    SingleInstanceGuard guard("test-key");
    ASSERT_TRUE(guard.tryToRun());

    QCoreApplication::processEvents();

    bool signalReceived = false;
    QEventLoop eventLoop;

    QObject::connect(&guard, &SingleInstanceGuard::instanceStarted,
                     [&signalReceived, &eventLoop]() {
                         signalReceived = true;
                         eventLoop.quit();
                     });

    // Setup timeout to avoid hanging
    QTimer::singleShot(1000, &eventLoop, &QEventLoop::quit);

    // Act - Try to activate from another instance
    SingleInstanceGuard guard2("test-key");
    bool activated = guard2.activateExistingInstance();

    // Wait for signal or timeout
    if (activated) {
        eventLoop.exec();
    }

    // Assert
    EXPECT_TRUE(signalReceived);
}

TEST_F(SingleInstanceGuardTest, ActivateReturnsCorrectStatus) {
    // Test when no instance is running
    {
        SingleInstanceGuard guard("test-key");
        bool activated = guard.activateExistingInstance();

        // Should fail because no instance is running
        EXPECT_FALSE(activated);
    }

    // Test when an instance is running
    {
        SingleInstanceGuard guard1("test-key");
        ASSERT_TRUE(guard1.tryToRun());

        // Give server time to start
        QCoreApplication::processEvents();

        SingleInstanceGuard guard2("test-key");
        bool activated = guard2.activateExistingInstance();

        // Process any pending events
        QCoreApplication::processEvents();

        // Should succeed because an instance is running
        EXPECT_TRUE(activated);
    }
}
