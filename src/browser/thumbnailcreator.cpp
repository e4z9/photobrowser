#include "thumbnailcreator.h"

#include "mediadirectorymodel.h"

#include <qtc/runextensions.h>

#include <QImageReader>
#include <QLoggingCategory>
#include <QTimer>
#include <QVideoSurfaceFormat>

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
    image = image.transformed(Util::matrixForOrientation(image.size(), orientation));
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
                                                   [future](const RunningItem &item) {
                                                       return item.second == future;
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

class VideoThumbnailCreator : public QAbstractVideoSurface
{
public:
    VideoThumbnailCreator(const QString &resolvedFilePath, int maxSize);

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
        QAbstractVideoBuffer::HandleType type = QAbstractVideoBuffer::NoHandle) const override;
    bool isFormatSupported(const QVideoSurfaceFormat &format) const override;
    bool start(const QVideoSurfaceFormat &format) override;
    bool present(const QVideoFrame &frame) override;

    QString resolvedFilePath() const;
    QFuture<ThumbnailItem> future();

private:
    void cancel();

    const QString m_resolvedFilePath;
    QFutureInterface<ThumbnailItem> m_fi;
    QFutureWatcher<ThumbnailItem> m_watcher;
    QMediaPlayer m_player;
    QImage::Format m_imageFormat;
    QSize m_imageSize;
    QRect m_imageRect;
    int m_maxSize;
    bool m_readyForSnapshot = false;
    bool m_snapshotDone = false;
};

VideoThumbnailCreator::VideoThumbnailCreator(const QString &resolvedFilePath, int maxSize)
    : m_resolvedFilePath(resolvedFilePath)
    , m_maxSize(maxSize)
{
    connect(&m_watcher, &QFutureWatcherBase::finished, this, &QObject::deleteLater);
    connect(&m_watcher, &QFutureWatcherBase::canceled, this, &VideoThumbnailCreator::cancel);
    m_watcher.setFuture(m_fi.future());
    m_fi.reportStarted();
    m_player.setVideoOutput(this);
    connect(&m_player,
            &QMediaPlayer::mediaStatusChanged,
            this,
            [this](const QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::InvalidMedia) {
                    m_fi.reportResult({});
                    cancel();
                }
                if (status == QMediaPlayer::LoadedMedia) {
                    QTimer::singleShot(0, this, [this] { m_player.play(); });
                }
            });
    m_player.setMedia(QUrl::fromLocalFile(resolvedFilePath));
    m_player.setMuted(true);
}

QList<QVideoFrame::PixelFormat> VideoThumbnailCreator::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType type) const
{
    if (type == QAbstractVideoBuffer::NoHandle)
        return {QVideoFrame::Format_RGB32,
                QVideoFrame::Format_ARGB32,
                QVideoFrame::Format_ARGB32_Premultiplied,
                QVideoFrame::Format_RGB565,
                QVideoFrame::Format_RGB555};
    return {};
}

bool VideoThumbnailCreator::isFormatSupported(const QVideoSurfaceFormat &format) const
{
    const QImage::Format imageFormat = QVideoFrame::imageFormatFromPixelFormat(format.pixelFormat());
    const QSize size = format.frameSize();

    return imageFormat != QImage::Format_Invalid && !size.isEmpty()
           && format.handleType() == QAbstractVideoBuffer::NoHandle;
}

bool VideoThumbnailCreator::start(const QVideoSurfaceFormat &format)
{
    const QImage::Format imageFormat = QVideoFrame::imageFormatFromPixelFormat(format.pixelFormat());
    const QSize size = format.frameSize();

    if (imageFormat != QImage::Format_Invalid && !size.isEmpty()) {
        m_imageFormat = imageFormat;
        m_imageSize = size;
        m_imageRect = format.viewport();
        QAbstractVideoSurface::start(format);
        return true;
    }
    return false;
}

bool VideoThumbnailCreator::present(const QVideoFrame &frame)
{
    static const qint64 snapshotTime = 10;
    if (m_snapshotDone)
        return false;
    if (!m_readyForSnapshot
        || (m_player.position() < snapshotTime
            && m_player.duration()
                   >= snapshotTime)) { // Delay once. First presentation is broken with black frame
        m_readyForSnapshot = true;
        return true;
    }
    if (surfaceFormat().pixelFormat() != frame.pixelFormat()
        || surfaceFormat().frameSize() != frame.size()) {
        setError(IncorrectFormatError);
        m_fi.reportResult({});
    }
    QVideoFrame currentFrame = frame;
    if (currentFrame.map(QAbstractVideoBuffer::ReadOnly)) {
        const QImage image(currentFrame.bits(),
                           currentFrame.width(),
                           currentFrame.height(),
                           currentFrame.bytesPerLine(),
                           m_imageFormat);
        QImage imageCopy(image);
        imageCopy.bits(); // detach / create copy of the pixel data
        currentFrame.unmap();
        m_fi.reportResult({restrictImageToSize(imageCopy, m_maxSize), m_player.duration()});
    }
    m_snapshotDone = true;
    stop();
    cancel();
    return false;
}

QString VideoThumbnailCreator::resolvedFilePath() const
{
    return m_resolvedFilePath;
}

QFuture<ThumbnailItem> VideoThumbnailCreator::future()
{
    return m_fi.future();
}

void VideoThumbnailCreator::cancel()
{
    m_player.setMedia({});
    m_fi.reportFinished();
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
    QPointer<VideoThumbnailCreator> m_creator;
};

MediaType VideoThumbnailer::mediaType() const
{
    return MediaType::Video;
}

bool VideoThumbnailer::hasCapacity() const
{
    return !m_creator;
}

bool VideoThumbnailer::isRunning(const QString &resolvedFilePath) const
{
    return m_creator && m_creator->resolvedFilePath() == resolvedFilePath;
}

void VideoThumbnailer::cancel(const QString &resolvedFilePath)
{
    if (m_creator && m_creator->resolvedFilePath() == resolvedFilePath) {
        qDebug(logThumb) << "canceling" << resolvedFilePath;
        m_creator->future().cancel();
        m_creator.clear();
    }
}

void VideoThumbnailer::requestThumbnail(const QString &resolvedFilePath,
                                        Util::Orientation orientation,
                                        const int maxSize)
{
    if (m_creator)
        cancel(m_creator->resolvedFilePath());
    qDebug(logThumb) << "starting" << resolvedFilePath;
    m_creator = new VideoThumbnailCreator(resolvedFilePath, maxSize);
    onFinished(m_creator->future(),
               this,
               [this, resolvedFilePath](const QFuture<ThumbnailItem> &future) {
                   qDebug(logThumb) << "finished" << resolvedFilePath;
                   m_creator.clear();
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
