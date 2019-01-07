#include "imageview.h"

#include <qtc/runextensions.h>

#include <QGraphicsPixmapItem>
#include <QGraphicsVideoItem>

static QImage imageForFilePath(const QString &filePath, Util::Orientation orientation)
{
    QImage image(filePath);
    return image.transformed(Util::matrixForOrientation(image.size(), orientation));
}

ImageView::ImageView()
    : m_player(nullptr, QMediaPlayer::VideoSurface)
{
    setScene(new QGraphicsScene(this));
    setTransformationAnchor(AnchorUnderMouse);
    setDragMode(ScrollHandDrag);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setRenderHint(QPainter::Antialiasing);
    setFocusPolicy(Qt::NoFocus);

    connect(&m_player,
            &QMediaPlayer::mediaStatusChanged,
            this,
            [this](const QMediaPlayer::MediaStatus status) {
                if (m_preloading
                    && (status == QMediaPlayer::BufferingMedia
                        || status == QMediaPlayer::BufferedMedia)) {
                    m_preloading = false;
                    QTimer::singleShot(0, this, [this] { m_player.pause(); });
                }
            });
}

void ImageView::clear()
{
    m_item = nullptr;
    scene()->clear();
    scene()->setSceneRect({0, 0, 0, 0});
    resetTransform();
}

void ImageView::setItem(const MediaItem &item)
{
    // TODO SVG
    m_loadingFuture.cancel();
    if (item.type == MediaType::Image) {
        m_loadingFuture = Utils::runAsync(imageForFilePath,
                                          item.resolvedFilePath,
                                          item.metaData ? item.metaData->orientation
                                                        : Util::Orientation::Normal);
        Utils::onResultReady(m_loadingFuture, this, [this](const QImage &image) {
            m_player.setMedia({});
            auto item = new QGraphicsPixmapItem(QPixmap::fromImage(image));
            item->setTransformationMode(Qt::SmoothTransformation);
            setItem(item);
            m_loadingFuture = QFuture<QImage>();
        });
    } else if (item.type == MediaType::Video) {
        m_player.setMedia({});
        auto grItem = new QGraphicsVideoItem;
        connect(grItem, &QGraphicsVideoItem::nativeSizeChanged, this, &ImageView::scaleToFit);
        m_player.setVideoOutput(grItem);
        m_player.setMedia(QUrl::fromLocalFile(item.resolvedFilePath));
        setItem(grItem);
        m_preloading = true;
        m_player.play();
    }
}

void ImageView::togglePlayVideo()
{
    if (m_player.state() != QMediaPlayer::PlayingState)
        m_player.play();
    else
        m_player.pause();
}

void ImageView::stepVideo(qint64 step)
{
    qint64 pos = m_player.position() + step;
    if (pos < 0)
        pos = 0;
    if (pos >= m_player.duration())
        pos = m_player.duration() - 1;
    m_player.setPosition(pos);
}

void ImageView::setItem(QGraphicsItem *item)
{
    clear();
    m_item = item;
    scene()->addItem(m_item);
    scene()->setSceneRect(m_item->boundingRect());
    scaleToFit();
}

void ImageView::scaleToFit()
{
    if (!m_item)
        return;
    fitInView(m_item, Qt::KeepAspectRatio);
}
