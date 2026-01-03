#include "kotodama/aimanager.h"
#include <QSettings>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QDebug>

// Placeholder URL - will be replaced with actual backend service
static const QString AI_SERVICE_URL = "https://api.kotodama.example.com/v1/define";

AIManager::AIManager()
    : networkManager(new QNetworkAccessManager(this))
    , currentReply(nullptr)
    , enabled(false)
{
    // Load enabled state from settings
    QSettings settings;
    enabled = settings.value("ai/enabled", false).toBool();
}

AIManager::~AIManager()
{
    if (currentReply) {
        currentReply->abort();
        currentReply->deleteLater();
    }
}

AIManager& AIManager::instance()
{
    static AIManager instance;
    return instance;
}

bool AIManager::isEnabled() const
{
    return enabled;
}

void AIManager::setEnabled(bool enable)
{
    if (enabled != enable) {
        enabled = enable;
        QSettings settings;
        settings.setValue("ai/enabled", enabled);
        emit enabledChanged();
    }
}

void AIManager::generateDefinition(const QString& term, const QString& language)
{
    // Cancel any existing request
    if (currentReply) {
        currentReply->abort();
        currentReply->deleteLater();
        currentReply = nullptr;
    }

    // Check if AI is enabled
    if (!enabled) {
        emit requestFailed("AI definitions are disabled. Enable them in Settings > AI.");
        return;
    }

    // Build request
    QNetworkRequest request = buildRequest();
    QByteArray requestBody = buildRequestBody(term, language);

    // Send request
    currentReply = networkManager->post(request, requestBody);
    connect(currentReply, &QNetworkReply::finished, this, &AIManager::onRequestFinished);

    // Set timeout (30 seconds)
    QTimer::singleShot(30000, this, [this]() {
        if (currentReply && currentReply->isRunning()) {
            currentReply->abort();
        }
    });
}

void AIManager::cancelRequest()
{
    if (currentReply) {
        currentReply->abort();
        currentReply->deleteLater();
        currentReply = nullptr;
    }
}

QNetworkRequest AIManager::buildRequest()
{
    QUrl url(AI_SERVICE_URL);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    return request;
}

QByteArray AIManager::buildRequestBody(const QString& term, const QString& language)
{
    QJsonObject body;
    body["term"] = term;
    body["language"] = language;

    QJsonDocument doc(body);
    return doc.toJson(QJsonDocument::Compact);
}

QString AIManager::parseResponse(const QByteArray& response)
{
    QJsonDocument doc = QJsonDocument::fromJson(response);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Failed to parse AI response as JSON";
        return QString();
    }

    QJsonObject obj = doc.object();
    if (!obj.contains("definition")) {
        qWarning() << "AI response missing 'definition' field";
        return QString();
    }

    QString definition = obj["definition"].toString().trimmed();
    return definition;
}

void AIManager::onRequestFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    // Handle network errors
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg;
        switch (reply->error()) {
            case QNetworkReply::ConnectionRefusedError:
            case QNetworkReply::RemoteHostClosedError:
            case QNetworkReply::HostNotFoundError:
                errorMsg = "Could not connect to AI service. Check your internet connection.";
                break;
            case QNetworkReply::TimeoutError:
                errorMsg = "Request timed out. Please try again.";
                break;
            case QNetworkReply::AuthenticationRequiredError:
                errorMsg = "Authentication failed. Please contact support.";
                break;
            case QNetworkReply::OperationCanceledError:
                // Request was cancelled, don't show error
                reply->deleteLater();
                currentReply = nullptr;
                return;
            default:
                errorMsg = QString("Network error: %1").arg(reply->errorString());
        }
        emit requestFailed(errorMsg);
        reply->deleteLater();
        currentReply = nullptr;
        return;
    }

    // Check HTTP status code
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode != 200) {
        QString errorMsg;
        switch (statusCode) {
            case 401:
                errorMsg = "Authentication failed. Please contact support.";
                break;
            case 429:
                errorMsg = "Rate limit exceeded. Please wait a moment and try again.";
                break;
            case 400:
                errorMsg = "Invalid request. This may be a bug - please report it.";
                break;
            case 500:
            case 502:
            case 503:
                errorMsg = "AI service is temporarily unavailable. Try again later.";
                break;
            default:
                errorMsg = QString("API error (code %1). Please try again.").arg(statusCode);
        }
        emit requestFailed(errorMsg);
        reply->deleteLater();
        currentReply = nullptr;
        return;
    }

    // Parse response
    QByteArray responseData = reply->readAll();
    QString definition = parseResponse(responseData);

    if (definition.isEmpty()) {
        emit requestFailed("Received empty or invalid response from AI service.");
    } else {
        emit definitionGenerated(definition);
    }

    reply->deleteLater();
    currentReply = nullptr;
}
