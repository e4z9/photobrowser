#include "filmrollview.h"

#include "fullscreensplitter.h"
#include "imageview.h"
#include "sqlabel.h"
#include "sqlistview.h"

#include <QAbstractItemDelegate>
#include <QEvent>
#include <QFont>
#include <QPainter>
#include <QScrollBar>
#include <QSplitter>
#include <QVBoxLayout>

using namespace sodium;

class Fotoroll : public SQListView
{
public:
    Fotoroll(const stream<boost::optional<int>> &sCurrentIndex);

    void setMediaModel(MediaDirectoryModel *model);
    bool event(QEvent *ev) override;

    const cell<OptionalMediaItem> &currentItem() const;

protected:
    void paintEvent(QPaintEvent *pe) override;

private:
    cell<OptionalMediaItem> m_currentItem;
    sodium::cell_sink<QString> m_frontDate; // date of the first visible item
    sodium::cell<QFont> m_frontDateFont;
    MediaItemDelegate m_delegate;
    SQLabel *m_dateLabel = nullptr;
};

static constexpr int MARGIN = 10;

FilmRollView::FilmRollView(const stream<boost::optional<int>> &sCurrentIndex,
                           const stream<unit> &sTogglePlayVideo,
                           const stream<qint64> &sStepVideo,
                           const stream<bool> &sFullscreen,
                           const stream<std::optional<qreal>> &sScale)
    : m_splitter(new FullscreenSplitter(sFullscreen))
    , m_fotoroll(new Fotoroll(sCurrentIndex))
{
    // delay change of current item in imageview
    const auto sStartSelectionTimer = m_fotoroll->currentItem().updates().map(
        [](const auto &) { return unit(); });
    m_selectionUpdate = std::make_unique<SQTimer>(sStartSelectionTimer);
    const cell<OptionalMediaItem> currentItem
        = m_selectionUpdate->sTimeout().snapshot(m_fotoroll->currentItem()).hold(std::nullopt);
    m_selectionUpdate->setInterval(80);
    m_selectionUpdate->setSingleShot(true);

    m_imageView = new ImageView(currentItem, sTogglePlayVideo, sStepVideo, sFullscreen, sScale);

    m_splitter->setOrientation(Qt::Vertical);
    m_splitter->setWidget(FullscreenSplitter::First, m_imageView);
    m_splitter->setWidget(FullscreenSplitter::Second, m_fotoroll);
    m_splitter->setFullscreenIndex(FullscreenSplitter::First);

    auto vLayout = new QVBoxLayout;
    vLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(vLayout);
    layout()->addWidget(m_splitter);
}

void FilmRollView::setModel(MediaDirectoryModel *model)
{
    m_fotoroll->setMediaModel(model);
}

MediaDirectoryModel *FilmRollView::model() const
{
    return static_cast<MediaDirectoryModel *>(m_fotoroll->model());
}

const sodium::cell<boost::optional<int>> &FilmRollView::currentIndex() const
{
    return m_fotoroll->cCurrentIndex();
}

const sodium::cell<OptionalMediaItem> &FilmRollView::currentItem() const
{
    return m_fotoroll->currentItem();
}

static QFont scaledFont(QFont font, int height)
{
    font.setPixelSize(std::max(5, std::min(height / 8, 12)));
    return font;
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

static int availableHeight(const QStyleOptionViewItem &option, bool showDateDisplay)
{
    if (!option.widget)
        return option.rect.height();
    const QFont dateFont = scaledFont(option.font, option.rect.height());
    return option.widget->height() - option.widget->style()->pixelMetric(QStyle::PM_ScrollBarExtent)
           - option.widget->style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarSpacing)
           - (showDateDisplay ? QFontMetrics(dateFont).height() : 0);
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

QRect MediaItemDelegate::paintThumbnail(QPainter *painter,
                                        const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
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
    const QFont durationFont = scaledFont(option.font, option.rect.height());
    paintDuration(painter, option.rect, durationFont, option.palette, durationStr);
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
    const auto showDateDisplay = index.data(int(MediaDirectoryModel::Role::ShowDateDisplay)).toBool();
    const int height = availableHeight(option, showDateDisplay);
    const QRect availableRect(option.rect.x(), option.rect.y(), option.rect.width(), height);
    if (option.state & QStyle::State_Selected) {
        const QPalette::ColorGroup group = (option.state & QStyle::State_HasFocus)
                                               ? QPalette::Active
                                               : QPalette::Inactive;
        painter->fillRect(availableRect, option.palette.brush(group, QPalette::Highlight));
    }
    const auto value = index.data(int(MediaDirectoryModel::Role::Item));
    if (!value.canConvert<MediaItem>())
        return;
    const auto item = value.value<MediaItem>();
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

    const QVariant dateDisplay = showDateDisplay
                                     ? index.data(int(MediaDirectoryModel::Role::DateDisplay))
                                     : QVariant();
    if (dateDisplay.isValid()) {
        const QFont dateFont = scaledFont(option.font, option.rect.height());
        painter->setFont(dateFont);
        painter->drawText(QPoint(option.rect.x() + MARGIN,
                                 option.rect.bottom() - QFontMetrics(dateFont).descent()),
                          dateDisplay.toString());
    }
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
    const auto showDateDisplay = index.data(int(MediaDirectoryModel::Role::ShowDateDisplay)).toBool();
    // TODO exiv2 might not be able to handle it, but Qt probably can (e.g. videos)
    return thumbnailSize(availableHeight(option, showDateDisplay), itemSize(item))
           + QSize(2 * MARGIN, 2 * MARGIN);
}

Fotoroll::Fotoroll(const stream<boost::optional<int>> &sCurrentIndex)
    : SQListView(sCurrentIndex)
    , m_currentItem(std::nullopt)
    , m_frontDate(QString())
    , m_frontDateFont(QFont())
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

    m_frontDateFont = font().lift(size(), [](const QFont &f, const QSize &s) {
        return scaledFont(f, s.height());
    });

    m_dateLabel = new SQLabel(this);
    m_dateLabel->setVisible(false);
    m_dateLabel->setAlignment(Qt::AlignCenter);
    m_dateLabel->setAutoFillBackground(true);
    QPalette pal = m_dateLabel->palette();
    pal.setBrush(QPalette::Window, pal.brush(QPalette::Base));
    m_dateLabel->setPalette(pal);
    m_dateLabel->text(m_frontDate);
    m_dateLabel->font(m_frontDateFont);
    const auto textSize
        = m_dateLabel->text().lift(m_dateLabel->font(), [](const QString &text, const QFont &font) {
              const QFontMetrics fm(font);
              return QSize(MARGIN + fm.horizontalAdvance(text), fm.height());
          });
    m_dateLabel->geometry(size().lift(textSize, [this](const QSize &rollSize, const QSize &textSize) {
        return QRect(1, rollSize.height() - textSize.height(), textSize.width(), textSize.height());
    }));
}

void Fotoroll::setMediaModel(MediaDirectoryModel *model)
{
    setModel(model);
    m_dateLabel->visible(model->showDateDisplay());
}

bool Fotoroll::event(QEvent *ev)
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

const cell<OptionalMediaItem> &Fotoroll::currentItem() const
{
    return m_currentItem;
}

void Fotoroll::paintEvent(QPaintEvent *pe)
{
    static int count = 0;
    SQListView::paintEvent(pe);
    if (!static_cast<MediaDirectoryModel *>(model())->showDateDisplay().sample())
        return;
    // paint date of first visible item
    const auto index = indexAt({0, 0});
    const auto value = index.data(int(MediaDirectoryModel::Role::Item));
    if (!value.canConvert<MediaItem>())
        return;
    const auto item = value.value<MediaItem>();
    const auto date = item.createdDateTime();
    m_frontDate.send(date.toString("d. MMMM yyyy"));
}
