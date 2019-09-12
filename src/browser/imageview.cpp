#include "imageview.h"

#include "gstreamer_utils.h"

#include <qtc/runextensions.h>

#include <QGestureEvent>
#include <QGraphicsObject>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QStackedLayout>

#include <QCoreApplication>
#include <QThread>
#include <QTimer>

#include <gst/gst.h>

Q_LOGGING_CATEGORY(logView, "browser.viewer", QtWarningMsg)

using namespace sodium;

static QImage imageForFilePath(const QString &filePath, Util::Orientation orientation)
{
    QImage image(filePath);
    return image.transformed(Util::matrixForOrientation(image.size(), orientation));
}

class VideoPlayer : public QObject
{
public:
    VideoPlayer(const cell<std::optional<QUrl>> &uri,
                const stream<unit> &sTogglePlayVideo,
                const stream<qint64> &sStepVideo);
    ~VideoPlayer() override = default;

    const cell<std::optional<QImage>> frame() const;

    // internal
    GstBusSyncReply message_cb(GstMessage *message);
    void fetchPreroll();
    void fetchNewSample();

private:
    void init();
    const int STATE_EOS = GST_STATE_PLAYING + 1;
    cell_sink<GstState> state;
    cell_sink<std::optional<QImage>> frame_sink;
    GstElementRef pipeline;
    GstElementRef source;
    GstElementRef sink;
    Unsubscribe m_unsubscribe;
};

static GstBusSyncReply vp_message_cb(GstBus *, GstMessage *m, gpointer player)
{
    return reinterpret_cast<VideoPlayer *>(player)->message_cb(m);
}

static GstFlowReturn new_preroll_cb(GstElement *, VideoPlayer *player)
{
    player->fetchPreroll();
    return GST_FLOW_OK;
}

static GstFlowReturn new_sample_cb(GstElement *, VideoPlayer *player)
{
    player->fetchNewSample();
    return GST_FLOW_OK;
}

VideoPlayer::VideoPlayer(const cell<std::optional<QUrl>> &uri,
                         const stream<unit> &sTogglePlayVideo,
                         const stream<qint64> &sStepVideo)
    : state(GST_STATE_NULL)
    , frame_sink(std::nullopt)
{
    pipeline.setCleanUp([](GstElement *e) {
        if (e)
            gst_element_set_state(e, GST_STATE_NULL);
    });
    init();
    m_unsubscribe += uri.listen(
        post<std::optional<QUrl>>(this, [this](const std::optional<QUrl> &uri) {
            transaction t;
            init();
            state.send(GST_STATE_NULL);
            frame_sink.send(std::nullopt);
            if (source())
                g_object_set(source(),
                             "uri",
                             (uri ? uri->toEncoded().constData() : nullptr),
                             nullptr);
            if (pipeline() && uri)
                gst_element_set_state(pipeline(), GST_STATE_PAUSED);
        }));
    m_unsubscribe += sStepVideo.listen(post<qint64>(this, [this](qint64 step) {
        if (!pipeline())
            return;
        gint64 position;
        if (gst_element_query_position(pipeline(), GST_FORMAT_TIME, &position)) {
            const GstSeekFlags snapOption = step < 0 ? GST_SEEK_FLAG_SNAP_BEFORE
                                                     : GST_SEEK_FLAG_SNAP_AFTER;
            gst_element_seek_simple(pipeline(),
                                    GST_FORMAT_TIME,
                                    GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT
                                                 | snapOption),
                                    std::max(0ll, position + GST_MSECOND * step));
        }
    }));
    m_unsubscribe += sTogglePlayVideo.snapshot(state).listen(post<
                                                             GstState>(this, [this](GstState state) {
        if (pipeline()) {
            if (state == STATE_EOS) {
                gst_element_seek_simple(pipeline(),
                                        GST_FORMAT_TIME,
                                        GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                                        0);
            }
            gst_element_set_state(pipeline(),
                                  state == GST_STATE_PAUSED || state == STATE_EOS
                                      ? GST_STATE_PLAYING
                                      : GST_STATE_PAUSED);
        }
    }));
}

const cell<std::optional<QImage>> VideoPlayer::frame() const
{
    return frame_sink;
}

GstBusSyncReply VideoPlayer::message_cb(GstMessage *message)
{
    if (GST_MESSAGE_SRC(message) == GST_OBJECT(pipeline())) {
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_STATE_CHANGED) {
            GstState newState;
            gst_message_parse_state_changed(message, nullptr, &newState, nullptr);
            state.send(newState);
        } else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS) {
            state.send(GstState(STATE_EOS));
        }
    }
    gst_message_unref(message);
    return GST_BUS_DROP;
}

void VideoPlayer::fetchPreroll()
{
    if (!sink())
        return;
    GstSample *sample;
    g_signal_emit_by_name(sink(), "pull-preroll", &sample, nullptr);
    const std::optional<QImage> image = imageFromGstSample(sample);
    gst_sample_unref(sample);
    if (image) // locking of send can interfere with locking of setting GST_STATE_NULL
        QMetaObject::invokeMethod(this, [this, image] { frame_sink.send(image); });
}

void VideoPlayer::fetchNewSample()
{
    if (!sink())
        return;
    GstSample *sample;
    g_signal_emit_by_name(sink(), "pull-sample", &sample, nullptr);
    const std::optional<QImage> image = imageFromGstSample(sample);
    gst_sample_unref(sample);
    if (image) // locking of send can interfere with locking of setting GST_STATE_NULL
        QMetaObject::invokeMethod(this, [this, image] { frame_sink.send(image); });
}

void VideoPlayer::init()
{
    sink.reset();
    source.reset();
    GError *error = nullptr;
    pipeline.reset(gst_parse_launch(
        "uridecodebin name=source "
        "source. ! queue ! videoconvert ! videoscale ! videoflip video-direction=auto ! "
        "appsink name=sink caps=\"video/x-raw,format=RGB,pixel-aspect-ratio=1/1\" "
        "source. ! queue ! audioconvert ! audioresample ! autoaudiosink",
        &error));
    if (error != nullptr) {
        qWarning(logView) << "gstreamer: failed to create pipeline \"" << error->message << "\"";
        g_error_free(error);
        return;
    }
    sink.reset(gst_bin_get_by_name(GST_BIN(pipeline()), "sink"));
    source.reset(gst_bin_get_by_name(GST_BIN(pipeline()), "source"));
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline()));
    gst_bus_set_sync_handler(bus, &vp_message_cb, this, nullptr);

    g_object_set(sink(), "emit-signals", true, nullptr);
    g_signal_connect(sink(), "new-preroll", G_CALLBACK(&new_preroll_cb), this);
    g_signal_connect(sink(), "new-sample", G_CALLBACK(&new_sample_cb), this);
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
    m_unsubscribe += this->image.listen(
        ensureSameThread<std::optional<QImage>>(qApp, [this](const auto &i) {
            if ((i && QRectF(i->rect()) != currentRect) || (!i && !currentRect.isNull())) {
                prepareGeometryChange();
                currentRect = i ? QRectF(i->rect()) : QRectF();
                if (rectCallback)
                    rectCallback(currentRect);
            }
            update();
        }));
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
    m_player = std::make_unique<VideoPlayer>(uri, sTogglePlayVideo, sStepVideo);
    m_item = new VideoGraphicsItem(m_player->frame());
    m_item->rectCallback = [this](const QRectF &r) {
        scene()->setSceneRect(r);
        fitInView(m_item, Qt::KeepAspectRatio);
    };
    scene()->addItem(m_item);

    m_unsubscribe += sFullscreen.map([](bool b) { return b ? QFrame::NoFrame : QFrame::Panel; })
                         .listen(ensureSameThread<QFrame::Shape>(this, &QFrame::setFrameShape));
    m_unsubscribe += sScale.listen([this](const std::optional<qreal> &s) {
        if (s)
            scale(*s, *s);
        else
            fitInView(m_item, Qt::KeepAspectRatio);
    });
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

    m_unsubscribe += image.listen(
        ensureSameThread<OptionalMediaItem>(this, [this](const OptionalMediaItem &i) {
            if (i)
                setItem(*i);
            else
                clear();
        }));
    m_unsubscribe += sFullscreen.map([](bool b) { return b ? QFrame::NoFrame : QFrame::Panel; })
                         .listen(ensureSameThread<QFrame::Shape>(this, &QFrame::setFrameShape));
    m_unsubscribe += sScale.listen([this](const std::optional<qreal> &s) {
        if (!m_item)
            return;
        if (s)
            scale(*s, *s);
        else
            fitInView(m_item, Qt::KeepAspectRatio);
    });
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
    const auto sFitAfterResize = m_scaleToFitTimer->sTimeout().map(
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

    m_unsubscribe += viewerWidget.listen(
        ensureSameThread<QWidget *>(m_layout, &QStackedLayout::setCurrentWidget));
    m_unsubscribe += sFullscreen.listen(ensureSameThread<bool>(this, [this](bool b) {
        auto p = palette();
        p.setColor(QPalette::Base, b ? Qt::black : QGuiApplication::palette().color(QPalette::Base));
        setPalette(p);
    }));

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
            m_sPinch.send(pg->scaleFactor());
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

bool ImageView::event(QEvent *ev)
{
    if (ev->type() == QEvent::Resize)
        m_sFitAfterResizeRequest.send({});
    return QWidget::event(ev);
}
