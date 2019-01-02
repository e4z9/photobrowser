#include "imageview.h"

#include <qtc/runextensions.h>

#include <QGraphicsPixmapItem>

static QImage imageForFilePath(const QString &filePath, Util::Orientation orientation)
{
    QImage image(filePath);
    return image.transformed(Util::matrixForOrientation(image.size(), orientation));
}

ImageView::ImageView()
{
    setScene(new QGraphicsScene(this));
    setTransformationAnchor(AnchorUnderMouse);
    setDragMode(ScrollHandDrag);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setRenderHint(QPainter::Antialiasing);
    setFocusPolicy(Qt::NoFocus);
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
    // TODO SVG and videos
    m_loadingFuture.cancel();
    m_loadingFuture = Utils::runAsync(imageForFilePath,
                                      item.resolvedFilePath,
                                      item.metaData ? item.metaData->orientation
                                                    : Util::Orientation::Normal);
    Utils::onResultReady(m_loadingFuture, this, [this](const QImage &image) {
        clear();
        auto item = new QGraphicsPixmapItem(QPixmap::fromImage(image));
        item->setTransformationMode(Qt::SmoothTransformation);
        m_item = item;
        scene()->addItem(m_item);
        scene()->setSceneRect(m_item->boundingRect());
        scaleToFit();
        m_loadingFuture = QFuture<QImage>();
    });
}

void ImageView::scaleToFit()
{
    if (!m_item)
        return;
    fitInView(m_item, Qt::KeepAspectRatio);
}
