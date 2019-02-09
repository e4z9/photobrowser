#pragma once

#include "../util/metadatautil.h"

#include <QAbstractItemModel>
#include <QAbstractVideoSurface>
#include <QDateTime>
#include <QFutureWatcher>
#include <QMediaPlayer>
#include <QSize>
#include <QTimer>

#include <optional.h>
#include <deque>

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
    std::optional<Util::MetaData> metaData;
    std::optional<qint64> duration;
    MediaType type;
};

QString durationToString(const qint64 durationMs);

using MediaItems = std::vector<MediaItem>;
Q_DECLARE_METATYPE(MediaItem)

class ThumbnailItem
{
public:
    QImage image;
    std::optional<qint64> duration;
};

class VideoSnapshotCreator : public QAbstractVideoSurface
{
    Q_OBJECT

public:
    static QFuture<ThumbnailItem> requestSnapshot(const QString &resolvedFilePath);

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
        QAbstractVideoBuffer::HandleType type = QAbstractVideoBuffer::NoHandle) const override;
    bool isFormatSupported(const QVideoSurfaceFormat &format) const override;
    bool start(const QVideoSurfaceFormat &format) override;
    bool present(const QVideoFrame &frame) override;

private:
    VideoSnapshotCreator(const QString &resolvedFilePath);
    void cancel();

    QFutureInterface<ThumbnailItem> m_fi;
    QFutureWatcher<ThumbnailItem> m_watcher;
    QMediaPlayer m_player;
    QImage::Format m_imageFormat;
    QSize m_imageSize;
    QRect m_imageRect;
    bool m_readyForSnapshot = false;
    bool m_snapshotDone = false;
};

class ThumbnailGoverner : public QObject
{
    Q_OBJECT
public:
    void requestThumbnail(const MediaItem &item, bool cancelRunning = false);
    void cancel(const QString &resolvedFilePath);

    using RunningItem = std::pair<QString, QFuture<ThumbnailItem>>;

signals:
    void thumbnailReady(const QString &resolvedFilePath,
                        const QPixmap &pixmap,
                        std::optional<qint64> duration);

private:
    void startItem(const QString &resolvedFilePath,
                    const MediaType type,
                    Util::Orientation orientation);
    void startPending();
    void logQueueSizes() const;

    std::deque<std::tuple<QString, MediaType, Util::Orientation>> m_pending;
    std::vector<RunningItem> m_running;
};

class MediaDirectoryModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum class Role { Item = Qt::UserRole, Thumbnail };
    enum class SortKey { ExifCreation, FileName, Random };

    MediaDirectoryModel();
    ~MediaDirectoryModel() override;

    void setPath(const QString &path, bool recursive);
    void moveItemAtIndexToTrash(const QModelIndex &index);
    void setSortKey(SortKey key);
    SortKey sortKey() const;

signals:
    void loadingStarted();
    void loadingFinished();

public:
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
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
    mutable ThumbnailGoverner m_thumbnailGoverner;
    SortKey m_sortKey = SortKey::ExifCreation;
    QString m_path;
    bool m_isRecursive;
};
