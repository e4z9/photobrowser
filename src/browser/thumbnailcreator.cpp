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

Q_LOGGING_CATEGORY(logGov, "browser.thumbnails", QtWarningMsg)
const int THUMBNAIL_SIZE = 400;

namespace {

QImage restrictImageToSize(const QImage &image, int maxSize)
{
    if (image.width() > maxSize || image.height() > maxSize) {
        if (image.width() > image.height())
            return image.scaledToWidth(maxSize, Qt::SmoothTransformation);
        return image.scaledToHeight(maxSize, Qt::SmoothTransformation);
    }
    return image;
}

void createThumbnailImage(QFutureInterface<ThumbnailItem> &fi,
                          const QString &filePath,
                          const Util::Orientation orientation,
                          const int maxSize)
{
    QImage image(filePath);
    if (fi.isCanceled())
        return;
    if (image.isNull()) {
        fi.reportResult({image, std::nullopt});
        return;
    }
    image = image.transformed(Util::matrixForOrientation(image.size(), orientation));
    if (fi.isCanceled())
        return;
    fi.reportResult({restrictImageToSize(image, maxSize), std::nullopt});
}

auto findRunningItem(std::vector<ThumbnailGoverner::RunningItem> &running,
                     const QString &resolvedFilePath)
{
    return std::find_if(running.begin(),
                        running.end(),
                        [resolvedFilePath](const ThumbnailGoverner::RunningItem &item) {
                            return item.first == resolvedFilePath;
                        });
}

auto findPendingItem(std::deque<std::tuple<QString, MediaType, Util::Orientation>> &pending,
                     const QString &resolvedFilePath)
{
    return std::find_if(pending.begin(),
                        pending.end(),
                        [resolvedFilePath](
                            const std::tuple<QString, MediaType, Util::Orientation> &item) {
                            return std::get<0>(item) == resolvedFilePath;
                        });
}

} // namespace

VideoSnapshotCreator::VideoSnapshotCreator(const QString &resolvedFilePath)
{
    connect(&m_watcher, &QFutureWatcherBase::finished, this, &QObject::deleteLater);
    connect(&m_watcher, &QFutureWatcherBase::canceled, this, &VideoSnapshotCreator::cancel);
    m_watcher.setFuture(m_fi.future());
    m_fi.reportStarted();
    m_player.setVideoOutput(this);
    connect(&m_player,
            &QMediaPlayer::mediaStatusChanged,
            this,
            [this](const QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::InvalidMedia)
                    m_fi.cancel();
                if (status == QMediaPlayer::LoadedMedia) {
                    QTimer::singleShot(0, this, [this] { m_player.play(); });
                }
            });
    m_player.setMedia(QUrl::fromLocalFile(resolvedFilePath));
    m_player.setMuted(true);
}

QList<QVideoFrame::PixelFormat> VideoSnapshotCreator::supportedPixelFormats(
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

bool VideoSnapshotCreator::isFormatSupported(const QVideoSurfaceFormat &format) const
{
    const QImage::Format imageFormat = QVideoFrame::imageFormatFromPixelFormat(format.pixelFormat());
    const QSize size = format.frameSize();

    return imageFormat != QImage::Format_Invalid && !size.isEmpty()
           && format.handleType() == QAbstractVideoBuffer::NoHandle;
}

bool VideoSnapshotCreator::start(const QVideoSurfaceFormat &format)
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

bool VideoSnapshotCreator::present(const QVideoFrame &frame)
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
        stop();
        m_fi.cancel();
        return false;
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
        m_fi.reportResult({restrictImageToSize(imageCopy, THUMBNAIL_SIZE), m_player.duration()});
    }
    m_snapshotDone = true;
    stop();
    cancel();
    return false;
}

void VideoSnapshotCreator::cancel()
{
    m_player.setMedia({});
    m_fi.reportFinished();
}

QFuture<ThumbnailItem> VideoSnapshotCreator::requestSnapshot(const QString &resolvedFilePath)
{
    return (new VideoSnapshotCreator(resolvedFilePath))->m_fi.future();
}

const int MAX_THREADS = 4;
const int MAX_PENDING = 40;

void ThumbnailGoverner::logQueueSizes() const
{
    qDebug(logGov) << "pending" << m_pending.size();
    qDebug(logGov) << "running" << m_running.size();
}

void ThumbnailGoverner::requestThumbnail(const MediaItem &item, bool cancelRunning)
{
    if (cancelRunning)
        cancel(item.resolvedFilePath);
    else if (findRunningItem(m_running, item.resolvedFilePath) != m_running.end())
        return;
    else if (findPendingItem(m_pending, item.resolvedFilePath) != m_pending.end())
        return;
    qDebug(logGov) << "requested" << item.resolvedFilePath << "("
                   << (item.type == MediaType::Image ? "Image" : "Video") << ")";
    if (m_running.size() >= MAX_THREADS) {
        while (m_pending.size() >= MAX_PENDING)
            m_pending.pop_front();
        m_pending.push_back({item.resolvedFilePath, item.type, item.metaData.orientation});
        qDebug(logGov) << "(scheduled)";
    } else {
        startItem(item.resolvedFilePath, item.type, item.metaData.orientation);
    }
    logQueueSizes();
}

void ThumbnailGoverner::cancel(const QString &resolvedFilePath)
{
    auto runningItem = findRunningItem(m_running, resolvedFilePath);
    if (runningItem != m_running.end()) {
        qDebug(logGov) << "canceled " << runningItem->first;
        runningItem->second.cancel();
        m_running.erase(runningItem);
        logQueueSizes();
    }
}

void ThumbnailGoverner::startItem(const QString &resolvedFilePath,
                                  const MediaType type,
                                  Util::Orientation orientation)
{
    auto future = type == MediaType::Image
                      ? Utils::runAsync(createThumbnailImage,
                                        resolvedFilePath,
                                        orientation,
                                        THUMBNAIL_SIZE)
                      : VideoSnapshotCreator::requestSnapshot(resolvedFilePath);
    m_running.emplace_back<RunningItem>({resolvedFilePath, future});
    qDebug(logGov) << "started  " << resolvedFilePath;
    logQueueSizes();
    onFinished(future, this, [this, resolvedFilePath](const QFuture<ThumbnailItem> &future) {
        auto runningItem = std::find_if(m_running.begin(),
                                        m_running.end(),
                                        [future](const RunningItem &item) {
                                            return item.second == future;
                                        });
        if (runningItem != m_running.end())
            m_running.erase(runningItem);
        else
            qWarning(logGov) << "Thumbnail governer internal error: Could not find running future";
        qDebug(logGov) << "finished " << resolvedFilePath;
        logQueueSizes();
        if (!future.isCanceled() && future.resultCount() > 0) {
            const auto result = future.result();
            emit thumbnailReady(resolvedFilePath, QPixmap::fromImage(result.image), result.duration);
        }
        startPending();
    });
}

void ThumbnailGoverner::startPending()
{
    if (m_pending.empty())
        return;
    const auto pending = m_pending.front();
    m_pending.pop_front();
    QString filePath;
    MediaType type;
    Util::Orientation orientation;
    std::tie(filePath, type, orientation) = pending;
    startItem(filePath, type, orientation);
}
