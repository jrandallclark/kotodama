#include "kotodama/singleinstanceguard.h"
#include "kotodama/constants.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QLocalSocket>
#include <QDebug>

SingleInstanceGuard::SingleInstanceGuard(const QString& key, QObject* parent)
    : QObject(parent)
    , instanceKey(key)
    , lockFile(nullptr)
    , localServer(nullptr)
{
}

SingleInstanceGuard::~SingleInstanceGuard()
{
    // Clean up server if it exists
    if (localServer) {
        localServer->close();
        delete localServer;
        localServer = nullptr;
    }

    // Clean up lock file if it exists
    if (lockFile) {
        if (lockFile->isLocked()) {
            lockFile->unlock();
        }
        delete lockFile;
        lockFile = nullptr;
    }
}

bool SingleInstanceGuard::tryToRun()
{
    // Create lock file path
    QString lockPath = getLockFilePath();
    lockFile = new QLockFile(lockPath);

    // Set stale lock time to 100ms (default is sufficient for crash detection)
    lockFile->setStaleLockTime(Constants::Timing::SHORT_TIMEOUT_MS);

    // Try to acquire the lock
    if (!lockFile->tryLock(Constants::Timing::SHORT_TIMEOUT_MS)) {
        qDebug() << "Failed to acquire lock - another instance is running";
        return false;
    }

    qDebug() << "Lock acquired successfully";

    // Create local server for IPC
    QString serverName = getServerName();

    // Remove any existing server (from previous crash)
    QLocalServer::removeServer(serverName);

    localServer = new QLocalServer(this);

    if (!localServer->listen(serverName)) {
        qWarning() << "Failed to start local server:" << localServer->errorString();
        qWarning() << "Continuing anyway - lock is the primary protection";
        // Don't return false - lock is the important part
    } else {
        qDebug() << "Local server started successfully";
        connect(localServer, &QLocalServer::newConnection,
                this, &SingleInstanceGuard::onNewConnection);
    }

    return true;
}

bool SingleInstanceGuard::isAnotherInstanceRunning()
{
    // If this object already holds the lock, no other instance is running
    if (lockFile && lockFile->isLocked()) {
        return false;
    }

    // Create a temporary lock file to test
    QString lockPath = getLockFilePath();
    QLockFile testLock(lockPath);
    testLock.setStaleLockTime(Constants::Timing::SHORT_TIMEOUT_MS);

    // Try to acquire the lock briefly
    if (testLock.tryLock(Constants::Timing::SHORT_TIMEOUT_MS)) {
        // We got the lock, so no other instance is running
        testLock.unlock();
        return false;
    }

    // Lock is held by another instance
    return true;
}

bool SingleInstanceGuard::activateExistingInstance()
{
    QString serverName = getServerName();
    QLocalSocket socket;

    socket.connectToServer(serverName);

    if (!socket.waitForConnected(Constants::Timing::LONG_TIMEOUT_MS)) {
        qWarning() << "Failed to connect to existing instance:" << socket.errorString();
        return false;
    }

    // Send activation message
    qint64 bytesWritten = socket.write("activate");
    if (bytesWritten == -1) {
        qWarning() << "Failed to write activation message:" << socket.errorString();
        return false;
    }

    // For local sockets, flush ensures data is sent
    if (!socket.flush()) {
        qWarning() << "Failed to flush socket";
        // Don't return false - data might still be sent
    }

    // Wait a bit for the message to be processed
    // Note: On local sockets, writes are usually immediate
    socket.waitForBytesWritten(Constants::Timing::MEDIUM_TIMEOUT_MS);

    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState) {
        socket.waitForDisconnected(Constants::Timing::LONG_TIMEOUT_MS);
    }

    qDebug() << "Activation message sent successfully";
    return true;
}

void SingleInstanceGuard::onNewConnection()
{
    QLocalSocket* clientSocket = localServer->nextPendingConnection();

    if (!clientSocket) {
        return;
    }

    // Lambda to handle reading data
    auto readData = [this, clientSocket]() {
        QByteArray data = clientSocket->readAll();

        if (data == "activate") {
            qDebug() << "Received activation request from another instance";
            emit instanceStarted();
        }

        clientSocket->disconnectFromServer();
        clientSocket->deleteLater();
    };

    // Connect readyRead for future data
    connect(clientSocket, &QLocalSocket::readyRead, this, readData);

    // Check if data is already available
    if (clientSocket->bytesAvailable() > 0) {
        readData();
    }

    connect(clientSocket, &QLocalSocket::disconnected, clientSocket, &QLocalSocket::deleteLater);
}

QString SingleInstanceGuard::getLockFilePath() const
{
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString orgName = QCoreApplication::organizationName();
    QString appName = QCoreApplication::applicationName();

    // Fallback if metadata not set
    if (orgName.isEmpty()) {
        orgName = "Unknown";
    }
    if (appName.isEmpty()) {
        appName = "Unknown";
    }

    return tempPath + "/" + orgName + "-" + appName + "-" + instanceKey + ".lock";
}

QString SingleInstanceGuard::getServerName() const
{
    // Use a simple, short name based on the instance key
    // This works better on macOS where local socket names have strict requirements
    return instanceKey;
}
