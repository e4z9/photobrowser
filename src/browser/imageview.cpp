#include "imageview.h"

#include <sqwidgetbase.h>
#include <util/util.h>
#include <qtc/runextensions.h>

#include <QAudioDevice>
#include <QAudioOutput>
#include <QCoreApplication>
#include <QGestureEvent>
#include <QGraphicsObject>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QLabel>
#include <QLoggingCategory>
#include <QMediaPlayer>
#include <QStackedLayout>
#include <QStyle>
#include <QThread>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>

Q_LOGGING_CATEGORY(logView, "browser.viewer", QtWarningMsg)

using namespace sodium;

static QSize sizeForString(const QString &s, const QFont &f)
{
    QFontMetrics fm(f);
    return fm.size(Qt::TextSingleLine, s) + QSize(1, 1);
}

void paintDuration(QPainter *painter,
                   const QRect &rect,
                   const QFont &font,
                   const QPalette &palette,
                   const QString &str)
{
    const QSize durationSize = sizeForString(str, font);
    const QPoint bottomRight = rect.bottomRight();
    const QRect durationRect(QPoint(bottomRight.x() - durationSize.width(),
                                    bottomRight.y() - durationSize.height()),
                             bottomRight);
    painter->fillRect(durationRect, palette.brush(QPalette::Base));
    painter->save();
    painter->setPen(palette.color(QPalette::Text));
    painter->setFont(font);
    painter->drawText(durationRect, Qt::AlignCenter, str);
    painter->restore();
}

static QImage imageForFilePath(const QString &filePath, Util::Orientation orientation)
{
    QImage image(filePath);
    return image.transformed(Util::matrixForOrientation(image.size(), orientation).toTransform());
}

class VideoPlayer : public QObject
{
public:
    VideoPlayer(const cell<std::optional<QUrl>> &uri,
                const stream<unit> &sTogglePlayVideo,
                const stream<qint64> &sStepVideo);
    ~VideoPlayer() override = default;

    const cell<std::optional<QImage>> frame() const;
    const cell<bool> &isPlaying() const;
    // seconds of position, in milliseconds
    const cell<std::optional<qint64>> &position() const;

private:
    void playForFrameGrabbing();
    void stopFromFrameGrabbing();

    cell_sink<std::optional<QImage>> m_frame_sink;
    cell_sink<std::optional<qint64>> m_position_sink;
    const cell_sink<bool> m_isPlaying_sink;
    Unsubscribe m_unsubscribe;
    QMediaPlayer m_player;
    QVideoSink m_videoSink;
    QAudioOutput m_audioOutput;
    bool m_playingForFrameGrabbing = false;
};

VideoPlayer::VideoPlayer(const cell<std::optional<QUrl>> &uri,
                         const stream<unit> &sTogglePlayVideo,
                         const stream<qint64> &sStepVideo)
    : m_frame_sink(std::nullopt)
    , m_position_sink(std::nullopt)
    , m_isPlaying_sink(false)
{
    m_audioOutput.setDevice(QAudioDevice()); // default system device
    m_player.setVideoSink(&m_videoSink);
    m_player.setAudioOutput(&m_audioOutput);
    connect(
        &m_player,
        &QMediaPlayer::playingChanged,
        this,
        [this](bool isPlaying) { m_isPlaying_sink.send(isPlaying); },
        Qt::QueuedConnection);
    connect(
        &m_player,
        &QMediaPlayer::positionChanged,
        this,
        [this](qint64 position) { m_position_sink.send(position); },
        Qt::QueuedConnection);
    connect(
        &m_videoSink,
        &QVideoSink::videoFrameChanged,
        this,
        [this](const QVideoFrame &frame) {
            if (!frame.isValid())
                return;
            m_frame_sink.send(frame.toImage());
            if (m_playingForFrameGrabbing)
                stopFromFrameGrabbing();
        },
        Qt::QueuedConnection);
    m_unsubscribe.insert_or_assign("uri",
                                   uri.listen(ensureSameThread<std::optional<QUrl>>(
                                       this, [this](const std::optional<QUrl> &uri) {
                                           m_player.setSource(uri.value_or(QUrl()));
                                           if (m_player.isAvailable()) {
                                               // get a first frame
                                               playForFrameGrabbing();
                                           }
                                       })));
    m_unsubscribe
        .insert_or_assign("stepvideo",
                          sStepVideo.listen(ensureSameThread<qint64>(this, [this](qint64 step) {
                              const qint64 current = m_player.position();
                              m_player.setPosition(qBound(0, current + step, m_player.duration()));
                              if (m_player.playbackState() == QMediaPlayer::StoppedState)
                                  playForFrameGrabbing();
                          })));
    m_unsubscribe.insert_or_assign("toggleplayvideo",
                                   sTogglePlayVideo.snapshot(m_isPlaying_sink)
                                       .listen(ensureSameThread<bool>(this, [this](bool isPlaying) {
                                           if (m_player.position() == m_player.duration()) {
                                               // Special case,
                                               // if the player is paused when at the end,
                                               // play() will directly stop again.
                                               // play from beginning.
                                               m_player.setPosition(0);
                                           }
                                           if (isPlaying)
                                               m_player.pause();
                                           else
                                               m_player.play();
                                       })));
}

const cell<std::optional<QImage>> VideoPlayer::frame() const
{
    return m_frame_sink;
}

const cell<bool> &VideoPlayer::isPlaying() const
{
    return m_isPlaying_sink;
}

const cell<std::optional<qint64>> &VideoPlayer::position() const
{
    return m_position_sink;
}

void VideoPlayer::playForFrameGrabbing()
{
    m_playingForFrameGrabbing = true;
    m_player.setAudioOutput(nullptr);
    m_player.play();
}

void VideoPlayer::stopFromFrameGrabbing()
{
    m_playingForFrameGrabbing = false;
    m_player.setAudioOutput(&m_audioOutput);
    m_player.pause();
}

class VideoGraphicsItem : public QGraphicsItem
{
public:
    VideoGraphicsItem(const cell<std::optional<QImage>> &image);

    QRectF boundingRect() const override;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    std::function<void(QRectF)> rectCallback;

private:
    cell<std::optional<QImage>> image;
    QRectF currentRect;
    Unsubscribe m_unsubscribe;
};

VideoGraphicsItem::VideoGraphicsItem(const cell<std::optional<QImage>> &image)
    : image(image)
{
    m_unsubscribe
        .insert_or_assign("image",
                          this->image.listen(
                              ensureSameThread<std::optional<QImage>>(qApp, [this](const auto &i) {
                                  if ((i && QRectF(i->rect()) != currentRect)
                                      || (!i && !currentRect.isNull())) {
                                      prepareGeometryChange();
                                      currentRect = i ? QRectF(i->rect()) : QRectF();
                                      if (rectCallback)
                                          rectCallback(currentRect);
                                  }
                                  update();
                              })));
}

QRectF VideoGraphicsItem::boundingRect() const
{
    return currentRect;
}

void VideoGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const auto i = image.sample();
    if (i)
        painter->drawImage(boundingRect(), *i);
}

class PlayIcon : public SQWidgetBase<QWidget>
{
public:
    PlayIcon(const cell<bool> &visible);

protected:
    void paintEvent(QPaintEvent *pe) override;
};

PlayIcon::PlayIcon(const cell<bool> &v)
{
    setVisible(v);
}

void PlayIcon::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    static const int SIZE = 64;
    const QPoint topLeft = rect().center() - QPoint(SIZE / 2, SIZE / 2);
    p.setPen(QColor(255, 255, 255, 200));
    p.setBrush(QColor(0, 0, 0, 100));
    p.drawPolygon(QPolygon(
        QVector<QPoint>({topLeft, topLeft + QPoint(SIZE, SIZE / 2), topLeft + QPoint(0, SIZE)})));
}

static const char kNoTimeString[] = "--:--";

class TimeDisplay : public QWidget
{
public:
    TimeDisplay(const cell<std::optional<qint64>> &position,
                const cell<std::optional<qint64>> &duration);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_timeString = kNoTimeString;
    Unsubscribe m_unsubscribe;
};

TimeDisplay::TimeDisplay(const cell<std::optional<qint64>> &position,
                         const cell<std::optional<qint64>> &duration)
{
    using time_type = const std::optional<qint64>;
    using time_pair_type = std::pair<time_type, time_type>;
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    const auto updateDisplay
        = ensureSameThread<time_pair_type>(this, [this](const time_pair_type &t) {
              const QString posStr = t.first ? durationToString(*t.first) : kNoTimeString;
              const QString durStr = t.second ? durationToString(*t.second) : kNoTimeString;
              m_timeString = posStr + " | " + durStr;
              updateGeometry();
              update();
          });
    m_unsubscribe.insert_or_assign("position",
                                   position
                                       .lift(duration,
                                             [](const time_type &p, const time_type &d) {
                                                 return time_pair_type(p, d);
                                             })
                                       .listen(updateDisplay));
}

QSize TimeDisplay::sizeHint() const
{
    return sizeForString(m_timeString, font());
}

void TimeDisplay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paintDuration(&p, rect(), font(), palette(), m_timeString);
}

class ScreenSleepBlocker
{
public:
    ScreenSleepBlocker(const cell<bool> &block);

private:
    Util::ScreenSleepBlocker m_blocker;
    Unsubscribe m_unsubscribe;
};

ScreenSleepBlocker::ScreenSleepBlocker(const cell<bool> &block)
    : m_blocker(ImageView::tr("playing video"))
{
    m_unsubscribe.insert_or_assign("block", calm(block).listen([this](bool block) {
        if (block)
            m_blocker.block();
        else
            m_blocker.unblock();
    }));
}

class VideoViewer : public QGraphicsView
{
public:
    VideoViewer(const cell<OptionalMediaItem> &video,
                const stream<unit> &sTogglePlayVideo,
                const stream<qint64> &sStepVideo,
                const stream<bool> &sFullscreen,
                const stream<std::optional<qreal>> &sScale);

private:
    Unsubscribe m_unsubscribe;
    VideoGraphicsItem *m_item = nullptr;
    std::unique_ptr<VideoPlayer> m_player;
    std::unique_ptr<ScreenSleepBlocker> m_screenSleepBlocker;
};

VideoViewer::VideoViewer(const cell<OptionalMediaItem> &video,
                         const stream<unit> &sTogglePlayVideo,
                         const stream<qint64> &sStepVideo,
                         const stream<bool> &sFullscreen,
                         const stream<std::optional<qreal>> &sScale)
{
    setScene(new QGraphicsScene(this));
    setTransformationAnchor(AnchorUnderMouse);
    setDragMode(ScrollHandDrag);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setRenderHint(QPainter::Antialiasing);
    setFocusPolicy(Qt::NoFocus);

    const cell<std::optional<QUrl>> uri = video.map([](const OptionalMediaItem &i) {
        return i ? std::make_optional<QUrl>(QUrl::fromLocalFile(i->resolvedFilePath))
                 : std::nullopt;
    });
    const cell<std::optional<qint64>> duration = video.map(
        [](const OptionalMediaItem &i) { return i ? i->metaData.duration : std::nullopt; });
    m_player = std::make_unique<VideoPlayer>(uri, sTogglePlayVideo, sStepVideo);
    m_screenSleepBlocker = std::make_unique<ScreenSleepBlocker>(m_player->isPlaying());
    m_item = new VideoGraphicsItem(m_player->frame());
    m_item->rectCallback = [this](const QRectF &r) {
        scene()->setSceneRect(r);
        fitInView(m_item, Qt::KeepAspectRatio);
    };
    scene()->addItem(m_item);

    m_unsubscribe
        .insert_or_assign("fullscreen",
                          sFullscreen
                              .map([](bool b) { return b ? QFrame::NoFrame : QFrame::Panel; })
                              .listen(
                                  ensureSameThread<QFrame::Shape>(this, &QFrame::setFrameShape)));
    m_unsubscribe.insert_or_assign("scale", sScale.listen([this](const std::optional<qreal> &s) {
        if (s)
            scale(*s, *s);
        else
            fitInView(m_item, Qt::KeepAspectRatio);
    }));

    auto layout = new QVBoxLayout;
    setLayout(layout);
    const int margin = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->addWidget(new PlayIcon(m_player->isPlaying().map([](bool b) { return !b; })), 10);
    layout->addStretch();
    layout->addWidget(new TimeDisplay(m_player->position(), duration));
}

class PictureViewer : public QGraphicsView
{
public:
    explicit PictureViewer(const cell<OptionalMediaItem> &image,
                           const stream<bool> &sFullscreen,
                           const stream<std::optional<qreal>> &sScale);

private:
    void clear();
    void setItem(const MediaItem &item);

    cell<OptionalMediaItem> m_image;
    Unsubscribe m_unsubscribe;
    QGraphicsItem *m_item = nullptr;
    QFuture<QImage> m_loadingFuture;
};

PictureViewer::PictureViewer(const cell<OptionalMediaItem> &image,
                             const stream<bool> &sFullscreen,
                             const stream<std::optional<qreal>> &sScale)
    : m_image(image)
{
    setScene(new QGraphicsScene(this));
    setTransformationAnchor(AnchorUnderMouse);
    setDragMode(ScrollHandDrag);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setRenderHint(QPainter::Antialiasing);
    setFocusPolicy(Qt::NoFocus);

    m_unsubscribe.insert_or_assign(
        "image",
        image.listen(ensureSameThread<OptionalMediaItem>(this, [this](const OptionalMediaItem &i) {
            if (i)
                setItem(*i);
            else
                clear();
        })));
    m_unsubscribe
        .insert_or_assign("fullscreen",
                          sFullscreen
                              .map([](bool b) { return b ? QFrame::NoFrame : QFrame::Panel; })
                              .listen(
                                  ensureSameThread<QFrame::Shape>(this, &QFrame::setFrameShape)));
    m_unsubscribe.insert_or_assign("scale", sScale.listen([this](const std::optional<qreal> &s) {
        if (!m_item)
            return;
        if (s)
            scale(*s, *s);
        else
            fitInView(m_item, Qt::KeepAspectRatio);
    }));
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
        fitInView(m_item, Qt::KeepAspectRatio);
        m_loadingFuture = QFuture<QImage>();
    });
}

static std::function<OptionalMediaItem(OptionalMediaItem)> itemIfOfType(MediaType type)
{
    return [type](const OptionalMediaItem &i) { return i && i->type == type ? i : std::nullopt; };
}

ImageView::ImageView(const cell<OptionalMediaItem> &item,
                     const stream<unit> &sTogglePlayVideo,
                     const stream<qint64> &sStepVideo,
                     const stream<bool> &sFullscreen,
                     const stream<std::optional<qreal>> &sScale)
    : m_item(item)
    , m_layout(new QStackedLayout)
{
    transaction trans;
    const cell<OptionalMediaItem> optImage = item.map(itemIfOfType(MediaType::Image));
    const cell<OptionalMediaItem> optVideo = item.map(itemIfOfType(MediaType::Video));
    const cell<bool> hasImage = optImage.map(&isMediaItem);
    const cell<bool> hasVideo = optVideo.map(&isMediaItem);

    m_scaleToFitTimer = std::make_unique<SQTimer>(m_sFitAfterResizeRequest.gate(m_isFittingInView));
    m_scaleToFitTimer->setSingleShot(true);
    m_scaleToFitTimer->setInterval(50);
    const auto sFitAfterResize = m_scaleToFitTimer->timedOut().map(
        [](unit) -> std::optional<qreal> { return {}; });

    const auto sScaleCombined = sScale.or_else(m_sPinch).or_else(sFitAfterResize);
    const auto sScaleIsFit = sScaleCombined.map([](const auto &s) { return !bool(s); });
    const auto sItemUpdated = item.updates().map([](const auto &) { return true; });
    m_isFittingInView.loop(sScaleIsFit.or_else(sItemUpdated).hold(true));

    auto pictureViewer = new PictureViewer(optImage, sFullscreen, sScaleCombined.gate(hasImage));

    auto videoViewer = new VideoViewer(optVideo,
                                       sTogglePlayVideo.gate(hasVideo),
                                       sStepVideo.gate(hasVideo),
                                       sFullscreen,
                                       sScaleCombined.gate(hasVideo));

    auto noViewer = new QWidget;
    m_layout->addWidget(pictureViewer);
    m_layout->addWidget(videoViewer);
    m_layout->addWidget(noViewer);
    const cell<QWidget *> viewerWidget
        = optImage.lift(optVideo,
                        [pictureViewer, videoViewer, noViewer](const OptionalMediaItem &img,
                                                               const OptionalMediaItem &vid) {
                            return img ? pictureViewer : vid ? videoViewer : noViewer;
                        });

    m_unsubscribe
        .insert_or_assign("viewerwidget",
                          viewerWidget.listen(
                              ensureSameThread<QWidget *>(m_layout,
                                                          &QStackedLayout::setCurrentWidget)));
    m_unsubscribe.insert_or_assign(
        "fullscreen", sFullscreen.listen(ensureSameThread<bool>(this, [this](bool b) {
            auto p = palette();
            p.setColor(QPalette::Base,
                       b ? Qt::black : QGuiApplication::palette().color(QPalette::Base));
            p.setColor(QPalette::Text,
                       b ? Qt::white : QGuiApplication::palette().color(QPalette::Text));
            setPalette(p);
        })));

    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);

    setFocusPolicy(Qt::NoFocus);

    grabGesture(Qt::PinchGesture);
    installEventFilter(this);
}

bool ImageView::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Gesture) {
        auto ge = static_cast<QGestureEvent *>(event);
        if (auto pg = static_cast<QPinchGesture *>(ge->gesture(Qt::PinchGesture)))
            post(this, [this, sf = pg->scaleFactor()] { m_sPinch.send(sf); });
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

bool ImageView::event(QEvent *ev)
{
    if (ev->type() == QEvent::Resize) {
        post(this, [this] { m_sFitAfterResizeRequest.send({}); });
    }
    return QWidget::event(ev);
}
