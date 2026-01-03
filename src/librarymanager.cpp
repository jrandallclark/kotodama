#include "kotodama/librarymanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QUuid>
#include <QDebug>

LibraryManager& LibraryManager::instance()
{
    static LibraryManager instance;
    return instance;
}

LibraryManager::LibraryManager()
{
}

LibraryManager::~LibraryManager()
{
}

bool LibraryManager::initialize()
{
    return ensureDirectoriesExist();
}

bool LibraryManager::ensureDirectoriesExist()
{
    QString libraryPath = getLibraryPath();
    QString textsPath = getTextsPath();

    QDir dir;

    if (!dir.exists(libraryPath)) {
        if (!dir.mkpath(libraryPath)) {
            qDebug() << "Failed to create library directory:" << libraryPath;
            return false;
        }
    }

    if (!dir.exists(textsPath)) {
        if (!dir.mkpath(textsPath)) {
            qDebug() << "Failed to create texts directory:" << textsPath;
            return false;
        }
    }

    return true;
}

QString LibraryManager::importText(const QString& sourceFilePath)
{
    QFile sourceFile(sourceFilePath);

    if (!sourceFile.exists()) {
        qDebug() << "Source file does not exist:" << sourceFilePath;
        return QString();
    }

    if (!sourceFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open source file:" << sourceFilePath;
        return QString();
    }

    // Generate UUID for the new file
    QString uuid = generateUuid();
    QString destPath = getTextFilePath(uuid);

    // Copy the file
    sourceFile.close();
    if (!QFile::copy(sourceFilePath, destPath)) {
        qDebug() << "Failed to copy file to:" << destPath;
        return QString();
    }

    return uuid;
}

QString LibraryManager::getTextFilePath(const QString& uuid)
{
    return getTextsPath() + "/" + uuid + ".txt";
}

bool LibraryManager::deleteText(const QString& uuid)
{
    QString filePath = getTextFilePath(uuid);
    QFile file(filePath);

    if (!file.exists()) {
        qDebug() << "File does not exist:" << filePath;
        return false;
    }

    if (!file.remove()) {
        qDebug() << "Failed to delete file:" << filePath;
        return false;
    }

    return true;
}

QString LibraryManager::getLibraryPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString LibraryManager::getTextsPath()
{
    return getLibraryPath() + "/texts";
}

qint64 LibraryManager::getFileSize(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.size();
}

int LibraryManager::getCharacterCount(const QString& filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Cannot open file for character count:" << filePath;
        return 0;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    return content.length();
}

QString LibraryManager::generateUuid()
{
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return uuid;
}
