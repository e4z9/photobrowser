#include "filmrollview.h"

#include "fullscreensplitter.h"
#include "imageview.h"
#include "sqlistview.h"

#include <QAbstractItemDelegate>
#include <QEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSplitter>
#include <QVBoxLayout>

using namespace sodium;

class Fotoroll : public SQListView
{
public:
    Fotoroll(const stream<boost::optional<int>> &sCurrentIndex)
        : SQListView(sCurrentIndex)
        , m_currentItem(std::nullopt)
    {
        setFlow(QListView::LeftToRight);
        setItemDelegate(&m_delegate);

        m_currentItem = cCurrentIndex().map([this](boost::optional<int> i) -> OptionalMediaItem {
            if (i) {
                const auto value = model()->data(model()->index(*i, 0),
                                                 int(MediaDirectoryModel::Role::Item));
                if (value.canConvert<MediaItem>())
                    return value.value<MediaItem>();
            }
            return {};
        });
    }

    bool event(QEvent *ev) override
    {
        if (ev->type() == QEvent::Resize) {
            transaction t; // avoid updating cells for deselecting and selecting
            const auto selection = selectionModel()->selection();
            const auto current = selectionModel()->currentIndex();
            // force relayout since we want the thumbnails to resize
            reset();
            selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
            selectionModel()->setCurrentIndex(current, QItemSelectionModel::Current);
        }
        return SQListView::event(ev);
    }

    const cell<OptionalMediaItem> &currentItem() const { return m_currentItem; }

private:
    cell<OptionalMediaItem> m_currentItem;
    MediaItemDelegate m_delegate;
};

static constexpr int MARGIN = 10;

FilmRollView::FilmRollView(const stream<unit> &sTogglePlayVideo,
                           const sodium::stream<qint64> &sStepVideo)
    : m_splitter(new FullscreenSplitter)
    , m_fotoroll(new Fotoroll(m_sCurrentIndex))
{
    // delay change of current item in imageview
    const auto sStartSelectionTimer = m_fotoroll->currentItem().updates().map(
        [](const auto &) { return unit(); });
    m_selectionUpdate = std::make_unique<SQTimer>(sStartSelectionTimer);
    const cell<OptionalMediaItem> currentItem
        = m_selectionUpdate->sTimeout().snapshot(m_fotoroll->currentItem()).hold(std::nullopt);
    m_selectionUpdate->setInterval(80);
    m_selectionUpdate->setSingleShot(true);

    m_imageView = new ImageView(currentItem, sTogglePlayVideo, sStepVideo);

    m_splitter->setOrientation(Qt::Vertical);
    m_splitter->setWidget(FullscreenSplitter::First, m_imageView);
    m_splitter->setWidget(FullscreenSplitter::Second, m_fotoroll);
    m_splitter->setFullscreenIndex(FullscreenSplitter::First);
    m_splitter->setFullscreenChangedAction(
        [this](bool fullscreen) { m_imageView->setFullscreen(fullscreen); });

    auto vLayout = new QVBoxLayout;
    vLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(vLayout);
    layout()->addWidget(m_splitter);
}

void FilmRollView::setModel(QAbstractItemModel *model)
{
    m_fotoroll->setModel(model);
}

QAbstractItemModel *FilmRollView::model() const
{
    return m_fotoroll->model();
}

void FilmRollView::zoomIn()
{
    m_imageView->scale(1.1);
}

void FilmRollView::zoomOut()
{
    m_imageView->scale(.9);
}

void FilmRollView::scaleToFit()
{
    m_imageView->scaleToFit();
}

void FilmRollView::previous()
{
    const auto currentIndex = m_fotoroll->cCurrentIndex().sample();
    if (!currentIndex && m_fotoroll->model()->rowCount() > 0)
        m_sCurrentIndex.send(0);
    else if (currentIndex && *currentIndex > 0)
        m_sCurrentIndex.send(*currentIndex - 1);
}

void FilmRollView::next()
{
    const auto currentIndex = m_fotoroll->cCurrentIndex().sample();
    const int rowCount = m_fotoroll->model()->rowCount();
    if (!currentIndex && rowCount > 0)
        m_sCurrentIndex.send(0);
    else if (currentIndex && *currentIndex < rowCount - 1)
        m_sCurrentIndex.send(*currentIndex + 1);
}

const sodium::cell<boost::optional<int>> &FilmRollView::currentIndex() const
{
    return m_fotoroll->cCurrentIndex();
}

const sodium::cell<OptionalMediaItem> &FilmRollView::currentItem() const
{
    return m_fotoroll->currentItem();
}

void FilmRollView::setFullscreen(bool fullscreen)
{
    m_splitter->setFullscreen(fullscreen);
}

static QSize thumbnailSize(const int viewHeight, const QSize dimensions)
{
    const int height = viewHeight - 2 * MARGIN;
    if (height <= 0)
        return {};
    const qreal factor = qreal(height) / dimensions.height();
    const int width = qMax(1, int(qRound(dimensions.width() * factor)));
    return {width, height};
}

static int availableHeight(const QStyleOptionViewItem &option)
{
    if (!option.widget)
        return option.rect.height();
    return option.widget->height() - option.widget->style()->pixelMetric(QStyle::PM_ScrollBarExtent)
           - option.widget->style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarSpacing);
}

static QSize defaultSize()
{
    static const QSize s{400, 300};
    return s;
}

MediaItemDelegate::MediaItemDelegate() = default;

static QRect thumbRect(const QStyleOptionViewItem &option, const QSize &size)
{
    return {option.rect.x() + MARGIN - 1, option.rect.y() + MARGIN - 1, size.width(), size.height()};
}

QRect MediaItemDelegate::paintThumbnail(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const auto thumbnail = index.data(int(MediaDirectoryModel::Role::Thumbnail)).value<QPixmap>();
    if (!thumbnail.isNull()) {
        const auto size = thumbnailSize(option.rect.height(), thumbnail.size());
        const QRect tRect = thumbRect(option, size);
        painter->drawPixmap(tRect, thumbnail);
        return tRect;
    }
    // fallback frame
    const auto value = index.data(int(MediaDirectoryModel::Role::Item));
    const auto item = value.value<MediaItem>();
    const auto size = thumbnailSize(option.rect.height(),
                                    item.metaData.dimensions ? *item.metaData.dimensions
                                                             : defaultSize());
    const QRect tRect = thumbRect(option, size);
    painter->save();
    painter->setPen(Qt::black);
    painter->drawRect(tRect);
    painter->restore();
    return tRect;
}

static void paintDuration(QPainter *painter,
                          const QStyleOptionViewItem &option,
                          const qint64 durationMs)
{
    const QString durationStr = durationToString(durationMs);
    QFont durationFont = option.font;
    durationFont.setPixelSize(std::min(option.rect.height() / 8, 12));
    QFontMetrics fm(durationFont);
    const QSize durationSize = fm.size(Qt::TextSingleLine, durationStr) + QSize(1, 1);
    const QPoint bottomRight = option.rect.bottomRight();
    const QRect durationRect(QPoint(bottomRight.x() - durationSize.width(),
                                    bottomRight.y() - durationSize.height()),
                             bottomRight);
    painter->fillRect(durationRect, option.palette.brush(QPalette::Base));
    painter->save();
    painter->setPen(Qt::black);
    painter->setFont(durationFont);
    painter->drawText(durationRect, Qt::AlignCenter, durationStr);
    painter->restore();
}

static void paintLink(QPainter *painter, const QStyleOptionViewItem &option)
{
    const qreal xr = qMin(option.rect.height() / 15, 6);
    const qreal yr = xr * 2 / 3;
    const qreal y = option.rect.bottom() - yr - MARGIN / 2;
    const qreal x = option.rect.x() + xr + MARGIN / 2;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(Qt::white, 5));
    painter->setBrush(Qt::white);
    const auto ellipse = [&painter, x, y, xr, yr] {
        painter->drawEllipse(QPointF(x, y), xr, yr);
        painter->drawEllipse(QPointF(x + xr * 4 / 3, y), xr, yr);
    };
    ellipse();
    painter->setPen(QPen(Qt::black, 1));
    painter->setBrush({});
    ellipse();
    painter->restore();
}

void MediaItemDelegate::paint(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    if (option.state & QStyle::State_Selected) {
        const QPalette::ColorGroup group = (option.state & QStyle::State_HasFocus)
                                               ? QPalette::Active
                                               : QPalette::Inactive;
        painter->fillRect(option.rect, option.palette.brush(group, QPalette::Highlight));
    }
    const auto value = index.data(int(MediaDirectoryModel::Role::Item));
    if (!value.canConvert<MediaItem>())
        return;
    const auto item = value.value<MediaItem>();
    const int height = availableHeight(option);
    const QRect availableRect(option.rect.x(), option.rect.y(), option.rect.width(), height);
    QStyleOptionViewItem fixedOpt = option;
    fixedOpt.rect = availableRect;

    const QRect tRect = paintThumbnail(painter, fixedOpt, index);

    if (item.metaData.duration && *item.metaData.duration > 0) {
        QStyleOptionViewItem opt = option;
        opt.rect = tRect;
        paintDuration(painter, opt, *item.metaData.duration);
    }

    if (item.filePath != item.resolvedFilePath)
        paintLink(painter, option);
}

static QSize itemSize(const MediaItem &item)
{
    if (item.thumbnail)
        return item.thumbnail->size();
    if (item.metaData.dimensions)
        return *item.metaData.dimensions;
    return defaultSize();
}

QSize MediaItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!option.widget)
        return {};
    const auto value = index.data(int(MediaDirectoryModel::Role::Item));
    if (!value.canConvert<MediaItem>())
        return {};
    const auto item = value.value<MediaItem>();
    // TODO exiv2 might not be able to handle it, but Qt probably can (e.g. videos)
    return thumbnailSize(availableHeight(option), itemSize(item)) + QSize(2 * MARGIN, 2 * MARGIN);
}
