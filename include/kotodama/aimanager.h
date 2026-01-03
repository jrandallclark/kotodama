#ifndef AIMANAGER_H
#define AIMANAGER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class AIManager : public QObject
{
    Q_OBJECT

public:
    static AIManager& instance();

    // Enable/disable AI features
    bool isEnabled() const;
    void setEnabled(bool enabled);

    // AI generation
    void generateDefinition(const QString& term, const QString& language);
    void cancelRequest();

signals:
    void definitionGenerated(QString definition);
    void requestFailed(QString errorMessage);
    void enabledChanged();

private:
    AIManager();
    ~AIManager();
    AIManager(const AIManager&) = delete;
    AIManager& operator=(const AIManager&) = delete;

    QNetworkAccessManager* networkManager;
    QNetworkReply* currentReply;
    bool enabled;

    // Helper methods
    QNetworkRequest buildRequest();
    QByteArray buildRequestBody(const QString& term, const QString& language);
    QString parseResponse(const QByteArray& response);

private slots:
    void onRequestFinished();
};

#endif // AIMANAGER_H
