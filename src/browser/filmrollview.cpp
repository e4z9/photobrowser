#include "filmrollview.h"

#include "imageview.h"
#include "mediadirectorymodel.h"

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

    MediaItemDelegate m_delegate;
};

static const int MARGIN = 10;

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
}

void FilmRollView::setModel(QAbstractItemModel *model)
{
    if (m_fotoroll->selectionModel())
        disconnect(m_fotoroll->selectionModel(), nullptr, this, nullptr);
    m_fotoroll->setModel(model);
    connect(m_fotoroll->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            [this](const QModelIndex &current) {
                if (!current.isValid())
                    m_imageView->clear();
                const auto value = m_fotoroll->model()->data(current,
                                                             int(MediaDirectoryModel::Role::Item));
                if (value.canConvert<MediaItem>()) {
                    m_imageView->setItem(value.value<MediaItem>());
                } else {
                    m_imageView->clear();
                }
            });
}

QAbstractItemModel *FilmRollView::model() const
{
    return m_fotoroll->model();
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
        painter->fillRect(option.rect, option.palette.brush(QPalette::Window));
    }
    const auto value = index.data(int(MediaDirectoryModel::Role::Item));
    if (!value.canConvert<MediaItem>())
        return;
    const auto item = value.value<MediaItem>();
    const auto size = thumbnailSize(availableHeight(option),
                                    item.metaData ? item.metaData->dimensions : defaultSize());
    const QRect thumbRect(option.rect.x() + MARGIN - 1,
                          option.rect.y() + MARGIN - 1,
                          size.width(),
                          size.height());
    if (item.metaData && item.metaData->thumbnail) {
        painter->drawPixmap(thumbRect, *(item.metaData->thumbnail));
    } else {
        painter->setPen(Qt::black);
        painter->drawRect(thumbRect);
    }
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
    return thumbnailSize(availableHeight(option),
                         item.metaData ? item.metaData->dimensions : defaultSize())
           + QSize(2 * MARGIN, 2 * MARGIN);
}

bool MediaItemDelegate::helpEvent(QHelpEvent *event,
                                  QAbstractItemView *view,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index)
{
    return false;
}
