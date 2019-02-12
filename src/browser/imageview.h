#pragma once

#include "mediadirectorymodel.h"

#include <QFuture>
#include <QGraphicsView>
#include <QImage>
#include <QMediaPlayer>
#include <QTimer>

#include <memory>

class ImageView : public QGraphicsView
{
public:
    ImageView();

    void clear();
    void setItem(const MediaItem &item);

    void togglePlayVideo();
    void stepVideo(qint64 step);

    void scaleToFit();
    void scale(qreal s);

    bool eventFilter(QObject *watched, QEvent *event) override;
    bool event(QEvent *ev) override;

private:
    void setItem(QGraphicsItem *item);

    QGraphicsItem *m_item = nullptr;
    QFuture<QImage> m_loadingFuture;
    QMediaPlayer m_player;
    QTimer m_scaleToFitTimer;
    bool m_preloading = false;
    bool m_scalingToFit = false;
};
