#pragma once

#include "mediadirectorymodel.h"

#include <QFuture>
#include <QGraphicsView>
#include <QImage>

#include <memory>

class ImageView : public QGraphicsView
{
public:
    ImageView();

    void clear();
    void setItem(const MediaItem &item);
    void scaleToFit();

private:
    QGraphicsItem *m_item = nullptr;
    QFuture<QImage> m_loadingFuture;
};
