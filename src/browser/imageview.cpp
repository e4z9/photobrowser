#include "imageview.h"

#include <QGraphicsPixmapItem>

static QGraphicsItem *itemForFilePath(const QString &filePath, Util::Orientation orientation)
{
    // TODO SVG and videos
    QPixmap pixmap(filePath);
    auto item = new QGraphicsPixmapItem(
                pixmap.transformed(Util::matrixForOrientation(pixmap.size(), orientation)));
    item->setTransformationMode(Qt::SmoothTransformation);
    return item;
}

ImageView::ImageView()
{
    setScene(new QGraphicsScene(this));
    setTransformationAnchor(AnchorUnderMouse);
    setDragMode(ScrollHandDrag);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setRenderHint(QPainter::Antialiasing);
}

void ImageView::clear()
{
    m_item = nullptr;
    scene()->clear();
    scene()->setSceneRect({0, 0, 0, 0});
    resetTransform();
}

void ImageView::setItem(const MediaItem &item)
{
    clear();
    // TODO async loading
    m_item = itemForFilePath(item.resolvedFilePath,
                             item.metaData ? item.metaData->orientation : Util::Orientation::Normal);
    scene()->addItem(m_item);
    scene()->setSceneRect(m_item->boundingRect());
    scaleToFit();
}

void ImageView::scaleToFit()
{
    if (!m_item)
        return;
    fitInView(m_item, Qt::KeepAspectRatio);
}
