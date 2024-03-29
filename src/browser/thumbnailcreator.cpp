#include "thumbnailcreator.h"

#include "mediadirectorymodel.h"

#include <qtc/runextensions.h>

#include <QImageReader>
#include <QLoggingCategory>
#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

template<typename T, typename Function>
const QFuture<T> &onFinished(const QFuture<T> &future, QObject *guard, Function f)
{
    auto watcher = new QFutureWatcher<T>();
    QObject::connect(watcher, &QFutureWatcherBase::finished, guard, [f, watcher] {
        f(watcher->future());
    });
    QObject::connect(watcher, &QFutureWatcherBase::finished, watcher, &QObject::deleteLater);
    watcher->setFuture(future);
    return future;
}

Q_LOGGING_CATEGORY(logThumb, "browser.thumbnails", QtWarningMsg)
const int THUMBNAIL_SIZE = 400;
const int MAX_PICTURE_THUMB_THREADS = 4;
const int MAX_PENDING = 40;

namespace {

class ThumbnailItem
{
public:
    QImage image;
    std::optional<qint64> duration;
};

QImage restrictImageToSize(const QImage &image, int maxSize)
{
    if (image.width() > maxSize || image.height() > maxSize) {
        if (image.width() > image.height())
            return image.scaledToWidth(maxSize, Qt::SmoothTransformation);
        return image.scaledToHeight(maxSize, Qt::SmoothTransformation);
    }
    return image;
}

void createThumbnailImage(QFutureInterface<QImage> &fi,
                          const QString &filePath,
                          const Util::Orientation orientation,
                          const int maxSize)
{
    QImage image(filePath);
    if (fi.isCanceled())
        return;
    if (image.isNull()) {
        fi.reportResult(image);
        return;
    }
    image = image.transformed(Util::matrixForOrientation(image.size(), orientation).toTransform());
    if (fi.isCanceled())
        return;
    fi.reportResult(restrictImageToSize(image, maxSize));
}

class PictureThumbnailer : public Thumbnailer
{
public:
    using RunningItem = std::pair<QString, QFuture<QImage>>;

    MediaType mediaType() const override;
    bool hasCapacity() const override;
    bool isRunning(const QString &resolvedFilePath) const override;
    void cancel(const QString &resolvedFilePath) override;
    void requestThumbnail(const QString &resolvedFilePath,
                          Util::Orientation orientation,
                          const int maxSize) override;

private:
    std::vector<RunningItem> m_running;
};

MediaType PictureThumbnailer::mediaType() const
{
    return MediaType::Image;
}

bool PictureThumbnailer::hasCapacity() const
{
    return m_running.size() < MAX_PICTURE_THUMB_THREADS;
}

bool PictureThumbnailer::isRunning(const QString &resolvedFilePath) const
{
    return std::find_if(m_running.begin(),
                        m_running.end(),
                        [resolvedFilePath](const PictureThumbnailer::RunningItem &item) {
                            return item.first == resolvedFilePath;
                        })
           != m_running.end();
}

void PictureThumbnailer::cancel(const QString &resolvedFilePath)
{
    auto runningItem = std::find_if(m_running.begin(),
                                    m_running.end(),
                                    [resolvedFilePath](const PictureThumbnailer::RunningItem &item) {
                                        return item.first == resolvedFilePath;
                                    });
    if (runningItem != m_running.end()) {
        qDebug(logThumb) << "canceling" << runningItem->first;
        runningItem->second.cancel();
        m_running.erase(runningItem);
    }
}

void PictureThumbnailer::requestThumbnail(const QString &resolvedFilePath,
                                          Util::Orientation orientation,
                                          const int maxSize)
{
    qDebug(logThumb) << "starting" << resolvedFilePath;
    auto future = Utils::runAsync(createThumbnailImage, resolvedFilePath, orientation, maxSize);
    m_running.emplace_back(resolvedFilePath, future);
    onFinished(future,
               this,
               [this, resolvedFilePath](const QFuture<QImage> &future) {
                   auto runningItem = std::find_if(m_running.begin(),
                                                   m_running.end(),
                                                   [resolvedFilePath](const RunningItem &item) {
                                                       return item.first == resolvedFilePath;
                                                   });
                   if (runningItem != m_running.end())
                       m_running.erase(runningItem);
                   else
                       qWarning(logThumb)
                           << "PictureThumbnailer internal error: Could not find running future";
                   if (!future.isCanceled() && future.resultCount() > 0) {
                       qDebug(logThumb) << "finished" << resolvedFilePath;
                       const auto result = future.result();
                       emit thumbnailReady(resolvedFilePath, result, std::nullopt);
                   }
               });
}

static void createVideoThumbnail(QFutureInterface<ThumbnailItem> &fi,
                                 const QString &resolvedFilePath,
                                 const int maxSize)
{
    const QUrl url = QUrl::fromLocalFile(resolvedFilePath);
    QMediaPlayer player;
    player.setSource(url);
    if (!player.isAvailable()) {
        qWarning(logThumb) << "QMediaPlayer: not available" << resolvedFilePath;
        return;
    }
    if (!player.hasVideo()) {
        qWarning(logThumb) << "QMediaPlayer: no video" << resolvedFilePath;
        return;
    }

    const qint64 duration = player.duration();
    const qint64 snapshotPos = duration * 3 / 100;
    player.setPosition(snapshotPos);
    QVideoSink sink;
    player.setVideoSink(&sink);
    QEventLoop loop;
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &loop, [&] {
        fi.reportResult({restrictImageToSize(sink.videoFrame().toImage(), maxSize), duration});
        loop.exit();
    });
    player.play();
    loop.exec();
}

class VideoThumbnailer : public Thumbnailer
{
public:
    MediaType mediaType() const override;
    bool hasCapacity() const override;
    bool isRunning(const QString &resolvedFilePath) const override;
    void cancel(const QString &resolvedFilePath) override;
    void requestThumbnail(const QString &resolvedFilePath,
                          Util::Orientation orientation,
                          const int maxSize) override;

private:
    bool isRunning() const { return m_future.isRunning(); }
    QString m_currentFilePath;
    QFuture<ThumbnailItem> m_future;
};

MediaType VideoThumbnailer::mediaType() const
{
    return MediaType::Video;
}

bool VideoThumbnailer::hasCapacity() const
{
    return !isRunning();
}

bool VideoThumbnailer::isRunning(const QString &resolvedFilePath) const
{
    return isRunning() && m_currentFilePath == resolvedFilePath;
}

void VideoThumbnailer::cancel(const QString &resolvedFilePath)
{
    if (isRunning(resolvedFilePath)) {
        qDebug(logThumb) << "canceling" << resolvedFilePath;
        m_future.cancel();
    }
}

void VideoThumbnailer::requestThumbnail(const QString &resolvedFilePath,
                                        Util::Orientation orientation,
                                        const int maxSize)
{
    Q_UNUSED(orientation)
    if (isRunning())
        cancel(m_currentFilePath);
    qDebug(logThumb) << "starting" << resolvedFilePath;
    m_currentFilePath = resolvedFilePath;
    QFutureInterface<ThumbnailItem> foo;
    m_future = Utils::runAsync(createVideoThumbnail, resolvedFilePath, maxSize);
    onFinished(m_future, this, [this, resolvedFilePath](const QFuture<ThumbnailItem> &future) {
        qDebug(logThumb) << "finished" << resolvedFilePath;
        if (!future.isCanceled() && future.resultCount() > 0) {
            const auto result = future.result();
            emit thumbnailReady(resolvedFilePath, result.image, result.duration);
        }
    });
}

} // namespace

static auto findPendingItem(std::deque<std::tuple<QString, MediaType, Util::Orientation>> &pending,
                            const QString &resolvedFilePath)
{
    return std::find_if(pending.begin(),
                        pending.end(),
                        [resolvedFilePath](
                            const std::tuple<QString, MediaType, Util::Orientation> &item) {
                            return std::get<0>(item) == resolvedFilePath;
                        });
}

ThumbnailCreator::ThumbnailCreator()
{
    m_thumbnailers.emplace(MediaType::Image, std::make_unique<PictureThumbnailer>());
    m_thumbnailers.emplace(MediaType::Video, std::make_unique<VideoThumbnailer>());
    for (const auto &thumbnailer : m_thumbnailers) {
        connect(thumbnailer.second.get(),
                &Thumbnailer::thumbnailReady,
                this,
                [this](const QString &resolvedFilePath,
                       const QImage &image,
                       std::optional<qint64> duration) {
                    emit thumbnailReady(resolvedFilePath, QPixmap::fromImage(image), duration);
                    startPending();
                });
    }
}

void ThumbnailCreator::requestThumbnail(const MediaItem &item, bool cancelRunning)
{
    if (cancelRunning)
        cancel(item.resolvedFilePath);
    if (isRunning(item.resolvedFilePath))
        return;
    if (findPendingItem(m_pending, item.resolvedFilePath) != m_pending.end())
        return;
    qDebug(logThumb) << "requested" << item.resolvedFilePath << "("
                     << (item.type == MediaType::Image ? "Image" : "Video") << ")";
    auto thumbnailer = m_thumbnailers.at(item.type).get();
    if (!thumbnailer->hasCapacity()) {
        while (m_pending.size() >= MAX_PENDING)
            m_pending.pop_front();
        m_pending.push_back({item.resolvedFilePath, item.type, item.metaData.orientation});
        qDebug(logThumb) << "(scheduled)";
    } else {
        startItem(item.resolvedFilePath, item.type, item.metaData.orientation);
    }
    qDebug(logThumb) << "pending" << m_pending.size();
}

void ThumbnailCreator::cancel(const QString &resolvedFilePath)
{
    for (const auto &thumbnailer : m_thumbnailers) {
        if (thumbnailer.second->isRunning(resolvedFilePath))
            thumbnailer.second->cancel(resolvedFilePath);
    }
}

bool ThumbnailCreator::isRunning(const QString &resolvedFilePath)
{
    return std::any_of(std::begin(m_thumbnailers),
                       std::end(m_thumbnailers),
                       [resolvedFilePath](const auto &thumbnailer) {
                           return thumbnailer.second->isRunning(resolvedFilePath);
                       });
}

void ThumbnailCreator::startItem(const QString &resolvedFilePath,
                                 const MediaType type,
                                 Util::Orientation orientation)
{
    auto thumbnailer = m_thumbnailers.at(type).get();
    thumbnailer->requestThumbnail(resolvedFilePath, orientation, THUMBNAIL_SIZE);
}

void ThumbnailCreator::startPending()
{
    if (m_pending.empty())
        return;
    auto it = std::begin(m_pending);
    const auto end = std::end(m_pending);
    while (it != end) {
        if (m_thumbnailers.at(std::get<1>(*it))->hasCapacity()) {
            QString filePath;
            MediaType type;
            Util::Orientation orientation;
            std::tie(filePath, type, orientation) = *it;
            m_pending.erase(it);
            startItem(filePath, type, orientation);
            break;
        }
        ++it;
    }
    qDebug(logThumb) << "pending" << m_pending.size();
}
