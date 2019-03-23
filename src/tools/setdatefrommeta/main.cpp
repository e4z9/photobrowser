#include <../../util/fileutil.h>
#include <../../util/metadatautil.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

#include <utime.h>
#include <sys/xattr.h>

static bool setTimes(const QString &filePath, const QDateTime &lastModified, const QDateTime &lastAccess)
{
    const utimbuf dt{lastAccess.toSecsSinceEpoch(), lastModified.toSecsSinceEpoch()};
    return 0 == utime(filePath.toUtf8().constData(), &dt);
}

// does not remove attributes that already are in target but not in source
static void copyXattr(const QString &source, const QString &target)
{
    const char *sourceFP = source.toUtf8().constData();
    const char *targetFP = target.toUtf8().constData();
    const auto entrySize = listxattr(sourceFP, nullptr, 0, XATTR_NOFOLLOW);
    char *namebuf = new char[entrySize];
    listxattr(sourceFP, namebuf, entrySize, XATTR_NOFOLLOW);

    auto currentName = namebuf;
    const auto end = namebuf + entrySize;
    while (currentName < end) {
        const auto valueSize = getxattr(sourceFP, currentName, nullptr, 0, 0, XATTR_NOFOLLOW);
        void *value = malloc(valueSize);
        getxattr(sourceFP, currentName, value, valueSize, 0, XATTR_NOFOLLOW);
        setxattr(targetFP, currentName, value, valueSize, 0, XATTR_NOFOLLOW);
        free(value);
        while (*currentName != '\0' && currentName < end) ++currentName;
        ++currentName; // skip \0
    }

    delete[] namebuf;
}

// creates copy with newer creation date
static std::optional<QString> createCopy(const QString &filePath)
{
    QFile source(filePath);
    if (!source.open(QIODevice::ReadOnly))
        return {};
    const QByteArray data = source.readAll();
    source.close();
    QTemporaryFile temp(filePath);
    temp.setAutoRemove(false);
    if (!temp.open())
        return {};
    temp.write(data);
    temp.close();
    temp.setPermissions(source.permissions());
    copyXattr(filePath, temp.fileName());
    return temp.fileName();
}

static bool safeReplace(const QString &source, const QString &target)
{
    const QString backupFilePath = target + '~';
    if (!QFile::rename(target, backupFilePath)) {
        QFile::remove(source);
        return false;
    }
    if (!QFile::rename(source, target)) {
        QFile::rename(backupFilePath, target);
        QFile::remove(source);
        return false;
    }
    QFile::remove(backupFilePath);
    return true;
}

static std::optional<QDateTime> targetDate(const QString &canonicalFilePath, bool isSymlink)
{
    const auto meta = Util::metaData(canonicalFilePath);
    if (meta.created)
        return meta.created;
    if (isSymlink)
        return QFileInfo(canonicalFilePath).lastModified();
    return {};
}

static bool resetCreationDateToMetaData(const QString &filePath)
{
    const QFileInfo fi(filePath);
    const QString canonicalFilePath = Util::resolveSymlinks(filePath);
    const auto dt = targetDate(canonicalFilePath, fi.isSymLink());
    if (!dt)
        return false;
    const QDateTime targetDateTime = *dt;
    const QDateTime birthTime = fi.birthTime();
    const QDateTime lastAccess = fi.lastRead();
    if (!birthTime.isValid()) {
        // birthTime not supported, set last modified date
        return setTimes(filePath, targetDateTime, lastAccess);
    } else if (birthTime < targetDateTime) {
        // need to re-create the file to get a newer creation date
        // TODO: different behavior for real symlinks. How to detect them compared to Aliases?
        const auto tempFilePath = createCopy(filePath);
        if (!tempFilePath)
            return false;
        if (!setTimes(*tempFilePath, targetDateTime, lastAccess)) {
            QFile::remove(*tempFilePath);
            return false;
        }
        return safeReplace(*tempFilePath, filePath);
    } else if (birthTime != targetDateTime) {
        return setTimes(filePath, targetDateTime, lastAccess);
    }
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    const auto args = QCoreApplication::arguments().mid(1);
    for (const QString &arg : args) {
        if (!resetCreationDateToMetaData(arg))
            qDebug() << "Failed to extract EXIF creation date for" << arg;
    }
    return 0;
}
