#pragma once

#include <QAbstractItemModel>
#include <QListView>
#include <QTimer>
#include <QWidget>

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
    bool helpEvent(QHelpEvent *event,
                   QAbstractItemView *view,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;
};

class FilmRollView : public QWidget
{
    Q_OBJECT
public:
    explicit FilmRollView(QWidget *parent = nullptr);

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;

private:
    void select(const QModelIndex &index);

    QListView *m_fotoroll;
    ImageView *m_imageView;
    QTimer m_selectionUpdate;
};
