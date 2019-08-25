#pragma once

#include "mediadirectorymodel.h"

#include <QAbstractItemModel>
#include <QListView>
#include <QTimer>
#include <QWidget>

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
    Q_OBJECT
public:
    explicit FilmRollView(QWidget *parent = nullptr);

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;

    void togglePlayVideo();
    void stepVideo(qint64 step);

    void zoomIn();
    void zoomOut();
    void scaleToFit();

    void previous();
    void next();

    QModelIndex currentIndex() const;
    OptionalMediaItem currentItem() const;

    void setFullscreen(bool fullscreen);

signals:
    void currentItemChanged();

private:
    void select(const QModelIndex &index);

    sodium::cell_sink<OptionalMediaItem> m_itemSink;
    FullscreenSplitter *m_splitter;
    Fotoroll *m_fotoroll;
    ImageView *m_imageView;
    QTimer m_selectionUpdate;
};
