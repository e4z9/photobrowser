#pragma once

#include "mediadirectorymodel.h"

#include <QGraphicsView>

class ImageView : public QGraphicsView
{
public:
    ImageView();

    void clear();
    void setItem(const MediaItem &item);
    void scaleToFit();

private:
    QGraphicsItem *m_item = nullptr;
};
