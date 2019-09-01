#pragma once

#include "thumbnailcreator.h"

#include "../util/metadatautil.h"

#include <QAbstractItemModel>
#include <QDateTime>
#include <QFutureWatcher>

#include <optional.h>

enum class MediaType { Image, Video };

class MediaItem
{
public:
    QString fileName;
    QString filePath;
    QString resolvedFilePath;
    QDateTime created;
    QDateTime lastModified;
    std::optional<QPixmap> thumbnail;
    Util::MetaData metaData;
    MediaType type;

    QString windowTitle() const;
};

using MediaItems = std::vector<MediaItem>;
using OptionalMediaItem = std::optional<MediaItem>;
bool isMediaItem(const OptionalMediaItem &item);
Q_DECLARE_METATYPE(MediaItem)

QString durationToString(const qint64 durationMs);
QString sizeToString(const qint64 size);

class MediaDirectoryModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum class Role { Item = Qt::UserRole, Thumbnail };
    enum class SortKey { ExifCreation, FileName, Random };

    MediaDirectoryModel();
    ~MediaDirectoryModel() override;

    void setPath(const QString &path, bool recursive);
    void moveItemAtIndexToTrash(int index);
    void setSortKey(SortKey key);
    SortKey sortKey() const;

signals:
    void loadingStarted();
    void loadingFinished();

public:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    using ResultList = std::vector<std::pair<int, MediaItems>>;

private:
    void insertItems(int index, const MediaItems &items);
    void cancelAndWait();

    MediaItems m_items;
    QFutureWatcher<ResultList> m_futureWatcher;
    mutable ThumbnailCreator m_thumbnailCreator;
    SortKey m_sortKey = SortKey::ExifCreation;
    QString m_path;
    bool m_isRecursive;
};
