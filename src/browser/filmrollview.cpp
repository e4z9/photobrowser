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
        return 0;
    return option.widget->height() - option.widget->style()->pixelMetric(QStyle::PM_ScrollBarExtent)
           - option.widget->style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarSpacing);
}

static QSize defaultSize()
{
    static const QSize s{400, 300};
    return s;
}

MediaItemDelegate::MediaItemDelegate() = default;

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
    const auto thumbRect = [option](const QSize &size) {
        return QRect(option.rect.x() + MARGIN - 1,
                     option.rect.y() + MARGIN - 1,
                     size.width(),
                     size.height());
    };
    const auto thumbnail = index.data(int(MediaDirectoryModel::Role::Thumbnail)).value<QPixmap>();
    QRect tRect;
    if (!thumbnail.isNull()) {
        const auto size = thumbnailSize(height, thumbnail.size());
        tRect = thumbRect(size);
        painter->drawPixmap(tRect, thumbnail);
    } else {
        const auto size = thumbnailSize(height,
                                        item.metaData ? item.metaData->dimensions : defaultSize());
        tRect = thumbRect(size);
        if (item.metaData && item.metaData->thumbnail) {
            painter->drawPixmap(tRect, *(item.metaData->thumbnail));
        } else {
            painter->setPen(Qt::black);
            painter->drawRect(tRect);
        }
    }

    if (item.duration && *item.duration > 0) {
        QTime duration(0, 0);
        duration = duration.addMSecs(*item.duration);
        const QString format = duration.hour() > 0 ? "HH:mm:ss" : "mm:ss";
        const QString durationStr = duration.toString(format);
        QFont durationFont = option.font;
        durationFont.setPixelSize(std::min(option.rect.height() / 10, 12));
        QFontMetrics fm(durationFont);
        const QSize durationSize = fm.size(Qt::TextSingleLine, durationStr) + QSize(1, 1);
        const QPoint bottomRight = tRect.bottomRight();
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

bool MediaItemDelegate::helpEvent(QHelpEvent *event,
                                  QAbstractItemView *view,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index)
{
    return false;
}
