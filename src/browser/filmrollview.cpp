#include "filmrollview.h"

#include "imageview.h"

#include <QAbstractItemDelegate>
#include <QEvent>
#include <QListView>
#include <QPainter>
#include <QScrollBar>
#include <QSplitter>
#include <QVBoxLayout>

class Fotoroll : public QListView
{
public:
    Fotoroll()
    {
        setFlow(QListView::LeftToRight);
        setItemDelegate(&m_delegate);
    }

    bool event(QEvent *ev) override
    {
        if (ev->type() == QEvent::Resize) {
            const auto selection = selectionModel()->selection();
            const auto current = selectionModel()->currentIndex();
            // force relayout since we want the thumbnails to resize
            reset();
            selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
            selectionModel()->setCurrentIndex(current, QItemSelectionModel::Current);
        }
        return QListView::event(ev);
    }

    void setModel(QAbstractItemModel *m) override
    {
        if (auto mediaModel = qobject_cast<MediaDirectoryModel *>(model()))
            disconnect(mediaModel, &MediaDirectoryModel::modelReset, this, nullptr);
        QListView::setModel(m);
        if (auto mediaModel = qobject_cast<MediaDirectoryModel *>(model())) {
            connect(mediaModel, &MediaDirectoryModel::modelReset, this, [this] {
                if (model()->rowCount() > 0) {
                    selectionModel()->setCurrentIndex(model()->index(0, 0),
                                                      QItemSelectionModel::SelectCurrent);
                }
            });
        }
    }

    MediaItemDelegate m_delegate;
};

static constexpr int MARGIN = 10;

FilmRollView::FilmRollView(QWidget *parent)
    : QWidget(parent)
{
    auto vLayout = new QVBoxLayout;
    vLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(vLayout);

    m_imageView = new ImageView;
    m_fotoroll = new Fotoroll;

    auto splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(m_imageView);
    splitter->addWidget(m_fotoroll);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);

    layout()->addWidget(splitter);

    m_selectionUpdate.setInterval(80);
    m_selectionUpdate.setSingleShot(true);
    connect(&m_selectionUpdate, &QTimer::timeout, this, [this] {
        select(m_fotoroll->selectionModel()->currentIndex());
    });
}

void FilmRollView::setModel(QAbstractItemModel *model)
{
    if (m_fotoroll->model())
        disconnect(m_fotoroll->model(), nullptr, this, nullptr);
    if (m_fotoroll->selectionModel())
        disconnect(m_fotoroll->selectionModel(), nullptr, this, nullptr);
    m_fotoroll->setModel(model);
    if (m_fotoroll->selectionModel()) {
        connect(m_fotoroll->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] {
            m_selectionUpdate.start();
            emit currentItemChanged();
        });
    }
    if (m_fotoroll->model()) {
        connect(m_fotoroll->model(), &QAbstractItemModel::modelReset, this, [this] {
            if (m_fotoroll->model()->rowCount() == 0) {
                m_selectionUpdate.start();
                emit currentItemChanged();
            }
        });
    }
}

QAbstractItemModel *FilmRollView::model() const
{
    return m_fotoroll->model();
}

void FilmRollView::togglePlayVideo()
{
    m_imageView->togglePlayVideo();
}

void FilmRollView::stepVideo(qint64 step)
{
    m_imageView->stepVideo(step);
}

void FilmRollView::zoomIn()
{
    m_imageView->scale(1.1, 1.1);
}

void FilmRollView::zoomOut()
{
    m_imageView->scale(.9, .9);
}

void FilmRollView::scaleToFit()
{
    m_imageView->scaleToFit();
}

QModelIndex FilmRollView::currentIndex() const
{
    return m_fotoroll->currentIndex();
}

std::optional<MediaItem> FilmRollView::currentItem() const
{
    const auto index = m_fotoroll->currentIndex();
    if (index.isValid()) {
        const auto value = m_fotoroll->model()->data(index, int(MediaDirectoryModel::Role::Item));
        if (value.canConvert<MediaItem>())
            return value.value<MediaItem>();
    }
    return {};
}

void FilmRollView::select(const QModelIndex &index)
{
    if (!index.isValid())
        m_imageView->clear();
    const auto value = m_fotoroll->model()->data(index, int(MediaDirectoryModel::Role::Item));
    if (value.canConvert<MediaItem>()) {
        m_imageView->setItem(value.value<MediaItem>());
    } else {
        m_imageView->clear();
    }
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
                                    item.metaData ? item.metaData->dimensions : defaultSize());
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

    if (item.duration && *item.duration > 0) {
        QStyleOptionViewItem opt = option;
        opt.rect = tRect;
        paintDuration(painter, opt, *item.duration);
    }

    if (item.filePath != item.resolvedFilePath)
        paintLink(painter, option);
}

static QSize itemSize(const MediaItem &item)
{
    if (item.thumbnail)
        return item.thumbnail->size();
    if (item.metaData)
        return item.metaData->dimensions;
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
