#ifndef SINGLEINSTANCEGUARD_H
#define SINGLEINSTANCEGUARD_H

#include <QObject>
#include <QLockFile>
#include <QLocalServer>
#include <QString>

/**
 * @brief SingleInstanceGuard prevents multiple instances of the application from running
 *
 * Uses QLockFile for robust instance detection with automatic crash recovery,
 * and QLocalServer for inter-process communication to activate existing instances.
 */
class SingleInstanceGuard : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new SingleInstanceGuard
     * @param key Unique identifier for this application instance
     * @param parent Parent QObject for memory management
     */
    explicit SingleInstanceGuard(const QString& key, QObject* parent = nullptr);

    /**
     * @brief Destructor - releases lock and cleans up server
     */
    ~SingleInstanceGuard();

    /**
     * @brief Check if this is the first instance and try to run
     * @return true if lock acquired and server started successfully
     * @return false if failed to acquire lock or start server
     */
    bool tryToRun();

    /**
     * @brief Check if another instance is already running
     * @return true if another instance holds the lock
     * @return false if no other instance detected
     */
    bool isAnotherInstanceRunning();

    /**
     * @brief Attempt to activate the existing instance
     * @return true if activation message sent successfully
     * @return false if failed to connect or send message
     */
    bool activateExistingInstance();

signals:
    /**
     * @brief Emitted when another instance tries to start
     *
     * Connect this to your main window's activation slot to bring
     * the window to the front when a second instance is launched.
     */
    void instanceStarted();

private slots:
    /**
     * @brief Handle new connection from another instance
     */
    void onNewConnection();

private:
    QString instanceKey;
    QLockFile* lockFile;
    QLocalServer* localServer;

    /**
     * @brief Get the path for the lock file
     * @return QString Path in temp directory with app-specific name
     */
    QString getLockFilePath() const;

    /**
     * @brief Get the server name for QLocalServer
     * @return QString Unique server name based on app metadata
     */
    QString getServerName() const;
};

#endif // SINGLEINSTANCEGUARD_H
