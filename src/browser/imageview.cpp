#include "imageview.h"

#include <qtc/runextensions.h>

#include <QGestureEvent>
#include <QGraphicsObject>
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

    viewport()->grabGesture(Qt::PinchGesture);
    viewport()->installEventFilter(this);

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
    m_scaleToFitTimer.setSingleShot(true);
    m_scaleToFitTimer.setInterval(50);
    connect(&m_scaleToFitTimer, &QTimer::timeout, this, &ImageView::scaleToFit);
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
                                          item.metaData.orientation);
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

bool ImageView::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Gesture) {
        auto ge = static_cast<QGestureEvent *>(event);
        if (auto pg = static_cast<QPinchGesture *>(ge->gesture(Qt::PinchGesture))) {
            m_scalingToFit = false;
            scale(pg->scaleFactor());
        }
        return true;
    }
    return QGraphicsView::eventFilter(watched, event);
}

bool ImageView::event(QEvent *ev)
{
    if (ev->type() == QEvent::Resize) {
        if (m_scalingToFit)
            m_scaleToFitTimer.start();
    }
    return QGraphicsView::event(ev);
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
    m_scalingToFit = true;
    if (!m_item)
        return;
    fitInView(m_item, Qt::KeepAspectRatio);
}

void ImageView::scale(qreal s)
{
    m_scalingToFit = false;
    QGraphicsView::scale(s, s);
}
