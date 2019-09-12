#pragma once

#include "mediadirectorymodel.h"

#include <sqtimer.h>
#include <sqtools.h>

#include <QAbstractItemModel>
#include <QListView>
#include <QTimer>
#include <QWidget>

#include <memory>
#include <optional.h>

#include <sodium/sodium.h>

class Fotoroll;
class FullscreenSplitter;
class ImageView;
class MediaDirectoryModel;

class MediaItemDelegate : public QAbstractItemDelegate
{
public:
    MediaItemDelegate();

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QRect paintThumbnail(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const;
};

class FilmRollView : public QWidget
{
public:
    FilmRollView(const sodium::stream<boost::optional<int>> &sCurrentIndex,
                 const sodium::stream<sodium::unit> &sTogglePlayVideo,
                 const sodium::stream<qint64> &sStepVideo,
                 const sodium::stream<bool> &sFullscreen,
                 const sodium::stream<std::optional<qreal>> &sScale);

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;

    const sodium::cell<boost::optional<int>> &currentIndex() const;
    const sodium::cell<OptionalMediaItem> &currentItem() const;

private:
    Unsubscribe m_unsubscribe;
    FullscreenSplitter *m_splitter;
    Fotoroll *m_fotoroll;
    ImageView *m_imageView;
    std::unique_ptr<SQTimer> m_selectionUpdate;
};
