#ifndef LIBRARYMANAGER_H
#define LIBRARYMANAGER_H

#include <QString>
#include <QFileInfo>

class LibraryManager
{
public:
    static LibraryManager& instance();

    bool initialize();

    // Returns UUID on success, empty string on failure
    QString importText(const QString& sourceFilePath);

    QString getTextFilePath(const QString& uuid);

    bool deleteText(const QString& uuid);

    QString getLibraryPath();
    QString getTextsPath();

    qint64 getFileSize(const QString& filePath);
    int getCharacterCount(const QString& filePath);

private:
    LibraryManager();
    ~LibraryManager();
    LibraryManager(const LibraryManager&) = delete;
    LibraryManager& operator=(const LibraryManager&) = delete;

    QString generateUuid();
    bool ensureDirectoriesExist();
};

#endif // LIBRARYMANAGER_H
