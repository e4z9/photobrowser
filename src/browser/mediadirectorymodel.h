#pragma once

#include "../util/metadatautil.h"

#include <QAbstractItemModel>
#include <QDateTime>
#include <QFuture>
#include <QSize>

#include <optional.h>
#include <queue>

class MediaItem
{
public:
    QString fileName;
    QString filePath;
    QString resolvedFilePath;
    QDateTime created;
    QDateTime lastModified;
    std::optional<QPixmap> thumbnail;
    std::optional<Util::MetaData> metaData;
};

using MediaItems = std::vector<MediaItem>;
Q_DECLARE_METATYPE(MediaItem)

class ThumbnailGoverner : public QObject
{
    Q_OBJECT
public:
    void requestThumbnail(const MediaItem &item, bool cancelRunning = false);
    void cancel(const QString &resolvedFilePath);

    using RunningItem = std::pair<QString, QFuture<QImage>>;
signals:
    void thumbnailReady(const QString &resolvedFilePath, const QPixmap &pixmap);

private:
    void start(const QString &resolvedFilePath, Util::Orientation orientation);
    void startPendingItem();
    void logQueueSizes() const;

    std::queue<std::pair<QString, Util::Orientation>> m_pending;
    std::vector<RunningItem> m_running;
};

class MediaDirectoryModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum class Role {
        Item = Qt::UserRole,
        Thumbnail
    };

    MediaDirectoryModel();

    void setPath(const QString &path);

signals:
    void loadingStarted();
    void loadingFinished();

public:
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    MediaItems m_items;
    QFuture<MediaItems> m_future;
    mutable ThumbnailGoverner m_thumbnailGoverner;
};
