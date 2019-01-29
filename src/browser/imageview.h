#pragma once

#include "mediadirectorymodel.h"

#include <QFuture>
#include <QGraphicsView>
#include <QImage>
#include <QMediaPlayer>

#include <memory>

class ImageView : public QGraphicsView
{
public:
    ImageView();

    void clear();
    void setItem(const MediaItem &item);

    void togglePlayVideo();
    void stepVideo(qint64 step);

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void scaleToFit();
    void setItem(QGraphicsItem *item);

    QGraphicsItem *m_item = nullptr;
    QFuture<QImage> m_loadingFuture;
    QMediaPlayer m_player;
    bool m_preloading = false;
};
