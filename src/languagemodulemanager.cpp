#include "kotodama/languagemodulemanager.h"
#include "kotodama/constants.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUrl>
#include <QDebug>

#include <miniz.h>

namespace {
const char* INSTALLED_MARKER = ".installed";

// Each language module is published as a separate release asset with a
// fixed filename so the URL stays stable across app versions.
QString moduleAssetName(const QString& langCode)
{
    return QStringLiteral("kotodama-module-%1.zip").arg(langCode);
}
}

LanguageModuleManager& LanguageModuleManager::instance()
{
    static LanguageModuleManager inst;
    return inst;
}

LanguageModuleManager::LanguageModuleManager()
    : networkManager(new QNetworkAccessManager(this))
    , currentReply(nullptr)
    , currentTempFile(nullptr)
{
}

LanguageModuleManager::~LanguageModuleManager()
{
    if (currentReply) {
        currentReply->abort();
        currentReply->deleteLater();
    }
    if (currentTempFile) {
        currentTempFile->remove();
        delete currentTempFile;
    }
}

bool LanguageModuleManager::isInstalled(const QString& langCode) const
{
    QFileInfo marker(QDir(modulePath(langCode)).absoluteFilePath(INSTALLED_MARKER));
    return marker.exists() && marker.isFile();
}

QString LanguageModuleManager::moduleUrl(const QString& langCode) const
{
    return QStringLiteral("%1/%2")
        .arg(QString::fromLatin1(Constants::Module::BASE_URL),
             moduleAssetName(langCode));
}

bool LanguageModuleManager::install(const QString& langCode)
{
    // Single source of truth for "busy". Set BEFORE any work that might
    // emit signals or fail asynchronously, so a re-entrant install() call
    // can't clobber currentLangCode mid-flight.
    if (busy) {
        return false;
    }
    busy = true;
    currentLangCode = langCode;

    // Ensure parent of staging dir exists.
    QDir().mkpath(QFileInfo(modulePath(langCode)).absolutePath());

    // Stage download into a temp file in the OS temp dir. Extraction goes
    // to a sibling staging dir so we can swap atomically on success without
    // ever touching an existing installed module on failure.
    auto* tmp = new QTemporaryFile(QDir::temp().filePath("kotodama-module-XXXXXX.zip"));
    tmp->setAutoRemove(false);
    if (!tmp->open()) {
        QString err = tmp->errorString();
        delete tmp;
        QMetaObject::invokeMethod(this, [this, err]() {
            finalizeFailure(QStringLiteral("Cannot create temp file: %1").arg(err));
        }, Qt::QueuedConnection);
        return true;
    }
    currentTempFile = tmp;

    QNetworkRequest req(QUrl(moduleUrl(langCode)));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    currentReply = networkManager->get(req);

    connect(currentReply, &QNetworkReply::downloadProgress,
            this, &LanguageModuleManager::onDownloadProgress);
    connect(currentReply, &QNetworkReply::readyRead, this, [this]() {
        if (currentTempFile && currentReply) {
            currentTempFile->write(currentReply->readAll());
        }
    });
    connect(currentReply, &QNetworkReply::finished,
            this, &LanguageModuleManager::onDownloadFinished);
    return true;
}

void LanguageModuleManager::cancel()
{
    // Only the download phase is cancellable. Extraction runs synchronously
    // on the main thread and takes ~1-3s for a 50MB dict — by the time the
    // user clicks Cancel during "Extracting…", currentReply is already
    // null and we let it finish. If extraction ever moves to a worker
    // thread, this is where we'd flag it for early exit.
    if (currentReply) {
        currentReply->abort();
    }
}

bool LanguageModuleManager::uninstall(const QString& langCode)
{
    QDir dir(modulePath(langCode));
    if (!dir.exists()) {
        return true;
    }
    return dir.removeRecursively();
}

void LanguageModuleManager::onDownloadProgress(qint64 received, qint64 total)
{
    emit progress(currentLangCode, received, total);
}

void LanguageModuleManager::onDownloadFinished()
{
    if (!currentReply) return;

    // Drain any remaining bytes.
    if (currentTempFile && currentReply->bytesAvailable() > 0) {
        currentTempFile->write(currentReply->readAll());
    }
    if (currentTempFile) {
        currentTempFile->flush();
        currentTempFile->close();
    }

    QNetworkReply::NetworkError err = currentReply->error();
    QString errStr = currentReply->errorString();
    QString tmpPath = currentTempFile ? currentTempFile->fileName() : QString();

    currentReply->deleteLater();
    currentReply = nullptr;

    if (err != QNetworkReply::NoError) {
        finalizeFailure(QStringLiteral("Download failed: %1").arg(errStr));
        return;
    }

    QFileInfo dlInfo(tmpPath);
    if (dlInfo.size() < 1024) {
        finalizeFailure(QStringLiteral("Downloaded file is too small (%1 bytes) — likely an error page")
                        .arg(dlInfo.size()));
        return;
    }

    emit extracting(currentLangCode);

    // Extract to a staging dir, then swap atomically so a failure can't
    // damage an already-installed module. Staging dir is removed first in
    // case a previous failed install left it behind.
    QString finalDir = modulePath(currentLangCode);
    QString stagingDir = finalDir + ".staging";
    QDir(stagingDir).removeRecursively();

    QString extractErr;
    if (!extractZip(tmpPath, stagingDir, &extractErr)) {
        QDir(stagingDir).removeRecursively();
        finalizeFailure(QStringLiteral("Extraction failed: %1").arg(extractErr));
        return;
    }

    if (!QFileInfo(stagingDir + "/dic/sys.dic").exists()) {
        QDir(stagingDir).removeRecursively();
        finalizeFailure(QStringLiteral("Module is missing dic/sys.dic after extraction"));
        return;
    }

    // Write installed marker into the staging dir before the swap. Bail
    // hard on failure: a swap without a marker means the module looks
    // uninstalled to isInstalled(), so the user would re-download every
    // time. Disk full or permission errors are recoverable on next try.
    QFile marker(stagingDir + "/" + INSTALLED_MARKER);
    if (!marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QString err = marker.errorString();
        QDir(stagingDir).removeRecursively();
        finalizeFailure(QStringLiteral("Could not write install marker: %1").arg(err));
        return;
    }
    marker.write(QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8());
    marker.close();

    // Swap: remove existing install, then rename staging into place.
    QDir(finalDir).removeRecursively();
    if (!QDir().rename(stagingDir, finalDir)) {
        QDir(stagingDir).removeRecursively();
        finalizeFailure(QStringLiteral("Could not move staged module into place"));
        return;
    }

    finalizeSuccess();
}

void LanguageModuleManager::finalizeFailure(const QString& message)
{
    QString lang = currentLangCode;
    if (currentTempFile) {
        currentTempFile->remove();
        delete currentTempFile;
        currentTempFile = nullptr;
    }
    // Don't touch modulePath(lang) — staged extraction goes to a sibling
    // dir, so any existing install is untouched on failure.
    currentLangCode.clear();
    busy = false;
    emit finished(lang, false, message);
}

void LanguageModuleManager::finalizeSuccess()
{
    QString lang = currentLangCode;
    if (currentTempFile) {
        currentTempFile->remove();
        delete currentTempFile;
        currentTempFile = nullptr;
    }
    currentLangCode.clear();
    busy = false;
    emit finished(lang, true, QString());
}

bool LanguageModuleManager::extractZip(const QString& zipPath,
                                       const QString& destDir,
                                       QString* error)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    QByteArray zipPathUtf8 = QFile::encodeName(zipPath);
    if (!mz_zip_reader_init_file(&zip, zipPathUtf8.constData(), 0)) {
        if (error) *error = QStringLiteral("cannot open archive");
        return false;
    }

    QDir().mkpath(destDir);
    bool ok = true;
    mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) {
            if (error) *error = QStringLiteral("entry stat failed");
            ok = false;
            break;
        }
        QString entryName = QString::fromUtf8(st.m_filename);
        // Reject path traversal.
        if (entryName.contains("..")) {
            if (error) *error = QStringLiteral("archive contains unsafe path: %1").arg(entryName);
            ok = false;
            break;
        }
        QString outPath = destDir + "/" + entryName;
        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            QDir().mkpath(outPath);
            continue;
        }
        QDir().mkpath(QFileInfo(outPath).absolutePath());
        QByteArray outPathUtf8 = QFile::encodeName(outPath);
        if (!mz_zip_reader_extract_to_file(&zip, i, outPathUtf8.constData(), 0)) {
            if (error) *error = QStringLiteral("extract failed for %1").arg(entryName);
            ok = false;
            break;
        }
    }

    mz_zip_reader_end(&zip);
    return ok;
}
