#pragma once

#include "mediadirectorymodel.h"

#include "sqtimer.h"
#include "tools.h"

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
    FilmRollView(const sodium::stream<sodium::unit> &sTogglePlayVideo,
                 const sodium::stream<qint64> &sStepVideo);

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;

    void zoomIn();
    void zoomOut();
    void scaleToFit();

    void previous();
    void next();

    void setFullscreen(bool fullscreen);

    const sodium::cell<boost::optional<int>> &currentIndex() const;
    const sodium::cell<OptionalMediaItem> &currentItem() const;

private:
    sodium::stream_sink<boost::optional<int>> m_sCurrentIndex;
    Unsubscribe m_unsubscribe;
    FullscreenSplitter *m_splitter;
    Fotoroll *m_fotoroll;
    ImageView *m_imageView;
    std::unique_ptr<SQTimer> m_selectionUpdate;
};
