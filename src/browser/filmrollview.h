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
    sodium::stream_sink<boost::optional<int>> m_sCurrentIndex;
    Unsubscribe m_unsubscribe;
    FullscreenSplitter *m_splitter;
    Fotoroll *m_fotoroll;
    ImageView *m_imageView;
    std::unique_ptr<SQTimer> m_selectionUpdate;
};
