#ifndef LANGUAGEMODULEMANAGER_H
#define LANGUAGEMODULEMANAGER_H

#include <QDir>
#include <QObject>
#include <QStandardPaths>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

// Manages on-demand language support modules (currently just the MeCab IPA
// dictionary for Japanese). Modules are downloaded as zip archives from a
// GitHub release and extracted under <AppDataLocation>/modules/<code>/.
//
// The presence of a `.installed` marker file is the source of truth for
// "is this module installed". Partial/aborted downloads are scrubbed before
// returning success.
class LanguageModuleManager : public QObject
{
    Q_OBJECT

public:
    static LanguageModuleManager& instance();

    // Path-only helpers — pure QString math, no singleton state. Inlined
    // so MeCabTokenizer can use them without linking the .cpp (which
    // would drag in QtNetwork). Single source of truth for "where does
    // module <code> live on disk".
    static QString modulePathFor(const QString& langCode) {
        return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                    + "/modules").absoluteFilePath(langCode);
    }
    static QString moduleDictPathFor(const QString& langCode) {
        return QDir(modulePathFor(langCode)).absoluteFilePath("dic");
    }

    bool isInstalled(const QString& langCode) const;
    QString modulePath(const QString& langCode) const { return modulePathFor(langCode); }
    QString moduleDictPath(const QString& langCode) const { return moduleDictPathFor(langCode); }

    // Begins async download. Emits progress() then finished(). Only one
    // install can be in flight at a time; calling install() while busy is
    // a no-op (returns false).
    bool install(const QString& langCode);

    // Cancels in-flight install (if any) for langCode. Cleans up partial state.
    void cancel();

    // Removes installed module files. Safe to call when not installed.
    bool uninstall(const QString& langCode);

signals:
    void progress(const QString& langCode, qint64 received, qint64 total);
    // Emitted after download completes and extraction begins. UI can switch
    // to an indeterminate progress indicator since extraction can take a
    // few seconds for a 50MB dict.
    void extracting(const QString& langCode);
    void finished(const QString& langCode, bool success, const QString& errorMessage);

private slots:
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();

private:
    LanguageModuleManager();
    ~LanguageModuleManager() override;
    LanguageModuleManager(const LanguageModuleManager&) = delete;
    LanguageModuleManager& operator=(const LanguageModuleManager&) = delete;

    QString moduleUrl(const QString& langCode) const;
    bool extractZip(const QString& zipPath, const QString& destDir, QString* error);
    void finalizeFailure(const QString& message);
    void finalizeSuccess();

    QNetworkAccessManager* networkManager;
    QNetworkReply* currentReply;
    QFile* currentTempFile;
    QString currentLangCode;
    bool busy = false;
};

#endif // LANGUAGEMODULEMANAGER_H
