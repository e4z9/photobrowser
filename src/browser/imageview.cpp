#include "imageview.h"

#include <qtc/runextensions.h>

#include <QGestureEvent>
#include <QGraphicsObject>
#include <QGraphicsPixmapItem>
#include <QGraphicsVideoItem>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QMediaPlayer>
#include <QStackedLayout>

static QImage imageForFilePath(const QString &filePath, Util::Orientation orientation)
{
    QImage image(filePath);
    return image.transformed(Util::matrixForOrientation(image.size(), orientation));
}

Viewer::~Viewer() {}

class VideoViewer : public QGraphicsView, public Viewer
{
public:
    explicit VideoViewer(QWidget *parent = nullptr);

    void clear() override;
    void setItem(const MediaItem &item) override;

    void togglePlayVideo() override;
    void stepVideo(qint64 step) override;

    void scaleToFit() override;
    bool isScalingToFit() const override;
    void scale(qreal s) override;

    void setFullscreen(bool fullscreen) override;

private:
    QGraphicsItem *m_item = nullptr;
    QMediaPlayer m_player;
    bool m_scalingToFit = false;
    bool m_preloading = false;
};

VideoViewer::VideoViewer(QWidget *parent)
    : QGraphicsView(parent)
    , m_player(nullptr, QMediaPlayer::VideoSurface)
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

void VideoViewer::clear()
{
    m_item = nullptr;
    scene()->clear();
    scene()->setSceneRect({0, 0, 0, 0});
    resetTransform();
    m_player.setMedia({});
}

void VideoViewer::setItem(const MediaItem &item)
{
    clear();
    auto grItem = new QGraphicsVideoItem;
    QObject::connect(grItem, &QGraphicsVideoItem::nativeSizeChanged, this, &VideoViewer::scaleToFit);
    m_player.setVideoOutput(grItem);
    m_player.setMedia(QUrl::fromLocalFile(item.resolvedFilePath));
    m_item = grItem;
    scene()->addItem(m_item);
    scene()->setSceneRect(m_item->boundingRect());
    scaleToFit();
    m_preloading = true;
    m_player.play();
}

void VideoViewer::togglePlayVideo()
{
    if (m_player.state() != QMediaPlayer::PlayingState)
        m_player.play();
    else
        m_player.pause();
}

void VideoViewer::stepVideo(qint64 step)
{
    qint64 pos = m_player.position() + step;
    if (pos < 0)
        pos = 0;
    if (pos >= m_player.duration())
        pos = m_player.duration() - 1;
    m_player.setPosition(pos);
}

void VideoViewer::scaleToFit()
{
    m_scalingToFit = true;
    if (!m_item)
        return;
    fitInView(m_item, Qt::KeepAspectRatio);
}

bool VideoViewer::isScalingToFit() const
{
    return m_scalingToFit;
}

void VideoViewer::scale(qreal s)
{
    m_scalingToFit = false;
    QGraphicsView::scale(s, s);
}

void VideoViewer::setFullscreen(bool fullscreen)
{
    setFrameShape(fullscreen ? QFrame::NoFrame : QFrame::Panel);
}

class PictureViewer : public QGraphicsView, public Viewer
{
public:
    explicit PictureViewer(QWidget *parent = nullptr);

    void clear() override;
    void setItem(const MediaItem &item) override;

    void togglePlayVideo() override;
    void stepVideo(qint64 step) override;

    void scaleToFit() override;
    bool isScalingToFit() const override;
    void scale(qreal s) override;

    void setFullscreen(bool fullscreen) override;

private:
    void setImage(const QString &filePath, Util::Orientation orientation);

    QGraphicsItem *m_item = nullptr;
    QFuture<QImage> m_loadingFuture;
    bool m_scalingToFit = false;
};

PictureViewer::PictureViewer(QWidget *parent)
    : QGraphicsView(parent)
{
    setScene(new QGraphicsScene(this));
    setTransformationAnchor(AnchorUnderMouse);
    setDragMode(ScrollHandDrag);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setRenderHint(QPainter::Antialiasing);
    setFocusPolicy(Qt::NoFocus);
}

void PictureViewer::clear()
{
    m_item = nullptr;
    scene()->clear();
    scene()->setSceneRect({0, 0, 0, 0});
    resetTransform();
}

void PictureViewer::setItem(const MediaItem &item)
{
    m_loadingFuture.cancel();
    m_loadingFuture = Utils::runAsync(imageForFilePath, item.filePath, item.metaData.orientation);
    Utils::onResultReady(m_loadingFuture, this, [this](const QImage &image) {
        auto item = new QGraphicsPixmapItem(QPixmap::fromImage(image));
        item->setTransformationMode(Qt::SmoothTransformation);
        clear();
        m_item = item;
        scene()->addItem(m_item);
        scene()->setSceneRect(m_item->boundingRect());
        scaleToFit();
        m_loadingFuture = QFuture<QImage>();
    });
}

void PictureViewer::togglePlayVideo()
{
    return;
}

void PictureViewer::stepVideo(qint64)
{
    return;
}

void PictureViewer::scaleToFit()
{
    m_scalingToFit = true;
    if (!m_item)
        return;
    fitInView(m_item, Qt::KeepAspectRatio);
}

bool PictureViewer::isScalingToFit() const
{
    return m_scalingToFit;
}

void PictureViewer::scale(qreal s)
{
    m_scalingToFit = false;
    QGraphicsView::scale(s, s);
}

void PictureViewer::setFullscreen(bool fullscreen)
{
    setFrameShape(fullscreen ? QFrame::NoFrame : QFrame::Panel);
}

ImageView::ImageView()
    : m_layout(new QStackedLayout)
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);

    auto pictureViewer = new PictureViewer;
    auto videoViewer = new VideoViewer;
    m_viewers.insert({MediaType::Image, pictureViewer});
    m_viewers.insert({MediaType::Video, videoViewer});
    m_layout->addWidget(pictureViewer);
    m_layout->addWidget(videoViewer);

    setFocusPolicy(Qt::NoFocus);

    grabGesture(Qt::PinchGesture);
    installEventFilter(this);

    m_scaleToFitTimer.setSingleShot(true);
    m_scaleToFitTimer.setInterval(50);
    connect(&m_scaleToFitTimer, &QTimer::timeout, this, &ImageView::scaleToFit);
}

void ImageView::clear()
{
    currentViewer()->clear();
}

void ImageView::setItem(const MediaItem &item)
{
    const int targetStackIndex = int(item.type);
    if (m_layout->currentIndex() != targetStackIndex) {
        currentViewer()->clear();
        m_layout->setCurrentIndex(targetStackIndex);
    }
    currentViewer()->setItem(item);
}

void ImageView::togglePlayVideo()
{
    currentViewer()->togglePlayVideo();
}

void ImageView::stepVideo(qint64 step)
{
    currentViewer()->stepVideo(step);
}

void ImageView::scaleToFit()
{
    currentViewer()->scaleToFit();
}

void ImageView::scale(qreal s)
{
    currentViewer()->scale(s);
}

void ImageView::setFullscreen(bool fullscreen)
{
    auto p = palette();
    p.setColor(QPalette::Base,
               fullscreen ? Qt::black : QGuiApplication::palette().color(QPalette::Base));
    setPalette(p);
    for (auto viewer : m_viewers)
        viewer.second->setFullscreen(fullscreen);
}

bool ImageView::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Gesture) {
        auto ge = static_cast<QGestureEvent *>(event);
        if (auto pg = static_cast<QPinchGesture *>(ge->gesture(Qt::PinchGesture)))
            currentViewer()->scale(pg->scaleFactor());
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

bool ImageView::event(QEvent *ev)
{
    if (ev->type() == QEvent::Resize) {
        if (currentViewer()->isScalingToFit())
            m_scaleToFitTimer.start();
    }
    return QWidget::event(ev);
}

Viewer *ImageView::currentViewer() const
{
    return m_viewers.at(MediaType(m_layout->currentIndex()));
}
