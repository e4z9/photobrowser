#pragma once

#include <util/metadatautil.h>

#include <QAbstractVideoSurface>
#include <QFutureWatcher>
#include <QMediaPlayer>

#include <deque>
#include <unordered_map>

class MediaItem;
enum class MediaType;

class Thumbnailer : public QObject
{
    Q_OBJECT

public:
    virtual MediaType mediaType() const = 0;
    virtual bool hasCapacity() const = 0;
    virtual bool isRunning(const QString &resolvedFilePath) const = 0;
    virtual void cancel(const QString &resolvedFilePath) = 0;
    virtual void requestThumbnail(const QString &resolvedFilePath,
                                  Util::Orientation orientation,
                                  const int maxSize)
        = 0;

signals:
    void thumbnailReady(const QString &resolvedFilePath,
                        const QImage &image,
                        std::optional<qint64> duration);
};

class ThumbnailCreator : public QObject
{
    Q_OBJECT

public:
    ThumbnailCreator();

    void requestThumbnail(const MediaItem &item, bool cancelRunning = false);

signals:
    void thumbnailReady(const QString &resolvedFilePath,
                        const QPixmap &pixmap,
                        std::optional<qint64> duration);

private:
    bool isRunning(const QString &resolvedFilePath);
    void cancel(const QString &resolvedFilePath);
    void startItem(const QString &resolvedFilePath,
                   const MediaType type,
                   Util::Orientation orientation);
    void startPending();

    std::deque<std::tuple<QString, MediaType, Util::Orientation>> m_pending;
    std::unordered_map<MediaType, std::unique_ptr<Thumbnailer>> m_thumbnailers;
};
