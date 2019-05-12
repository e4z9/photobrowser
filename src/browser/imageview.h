#pragma once

#include "mediadirectorymodel.h"

#include <QTimer>
#include <QWidget>

#include <unordered_map>

QT_BEGIN_NAMESPACE
class QStackedLayout;
QT_END_NAMESPACE

class Viewer
{
public:
    virtual ~Viewer();

    virtual void clear() = 0;
    virtual void setItem(const MediaItem &item) = 0;

    virtual void togglePlayVideo() = 0;
    virtual void stepVideo(qint64 step) = 0;

    virtual void scaleToFit() = 0;
    virtual bool isScalingToFit() const = 0;
    virtual void scale(qreal s) = 0;

    virtual void setFullscreen(bool fullscreen) = 0;
};

class ImageView : public QWidget
{
public:
    ImageView();

    void clear();
    void setItem(const MediaItem &item);

    void togglePlayVideo();
    void stepVideo(qint64 step);

    void scaleToFit();
    void scale(qreal s);

    void setFullscreen(bool fullscreen);

    bool eventFilter(QObject *watched, QEvent *event) override;
    bool event(QEvent *ev) override;

private:
    Viewer *currentViewer() const;

    QTimer m_scaleToFitTimer;
    std::unordered_map<MediaType, Viewer *> m_viewers;
    QStackedLayout *m_layout;
};
