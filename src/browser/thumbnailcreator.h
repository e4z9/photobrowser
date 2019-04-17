#pragma once

#include "../util/metadatautil.h"

#include <QAbstractVideoSurface>
#include <QFutureWatcher>
#include <QMediaPlayer>

#include <deque>

class MediaItem;
enum class MediaType;

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
