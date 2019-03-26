#pragma once

#include "mediadirectorymodel.h"

#include <QAbstractItemModel>
#include <QListView>
#include <QTimer>
#include <QWidget>

#include <optional.h>

class Fotoroll;
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
    std::optional<MediaItem> currentItem() const;

signals:
    void currentItemChanged();

private:
    void select(const QModelIndex &index);

    Fotoroll *m_fotoroll;
    ImageView *m_imageView;
    QTimer m_selectionUpdate;
};
