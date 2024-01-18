#pragma once

#include "thumbnailcreator.h"

#include <sqtools.h>

#include <util/metadatautil.h>

#include <QAbstractItemModel>
#include <QDateTime>
#include <QFutureWatcher>

#include <sodium/sodium.h>

#include <optional>

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

    mutable QDateTime cachedCreatedDateTime;
    const QDateTime &createdDateTime() const;
    QString windowTitle() const;
};

using MediaItems = std::vector<MediaItem>;
using OptionalMediaItem = std::optional<MediaItem>;
bool isMediaItem(const OptionalMediaItem &item);
Q_DECLARE_METATYPE(MediaItem)

QString durationToString(const qint64 durationMs);
QString sizeToString(const qint64 size);

DEFINE_BOOL_TYPE(IsRecursive)

class MediaDirectoryModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    struct Filter
    {
        QString searchString;
        bool videosOnly;
    };

    enum class Role { Item = Qt::UserRole, Thumbnail, ShowDateDisplay, DateDisplay };
    enum class SortKey { ExifCreation, FileName, Random };

    MediaDirectoryModel(const sodium::cell<QString> &path,
                        const sodium::cell<IsRecursive> &isRecursive,
                        const sodium::cell<Filter> &filter,
                        const sodium::cell<SortKey> &sortKey);
    ~MediaDirectoryModel() override;

    const sodium::stream<sodium::unit> &sLoadingStarted() const;
    const sodium::stream<sodium::unit> &sLoadingFinished() const;

    const sodium::cell<bool> &showDateDisplay() const;

    void moveItemAtIndexToTrash(int index);

public:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    using ResultList = std::vector<std::pair<int, MediaItems>>;

private:
    void load();
    void setSortKey(SortKey key);
    void insertItems(int index, const MediaItems &items);
    void cancelAndWait();

    MediaItems m_items;
    QFutureWatcher<ResultList> m_futureWatcher;
    mutable ThumbnailCreator m_thumbnailCreator;
    sodium::cell<QString> m_path;
    sodium::cell<IsRecursive> m_isRecursive;
    sodium::cell<Filter> m_filter;
    sodium::cell<SortKey> m_sortKey;
    sodium::cell<bool> m_showDateDisplay;
    sodium::stream_sink<sodium::unit> m_sLoadingStarted;
    sodium::stream_sink<sodium::unit> m_sLoadingFinished;
    Unsubscribe m_unsubscribe;
};
