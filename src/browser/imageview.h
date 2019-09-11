#pragma once

#include "mediadirectorymodel.h"

#include <sqtools.h>

#include <QTimer>
#include <QWidget>

#include <optional.h>

#include <sodium/sodium.h>

#include <unordered_map>

QT_BEGIN_NAMESPACE
class QStackedLayout;
QT_END_NAMESPACE

class Viewer;

class ImageView : public QWidget
{
public:
    ImageView(const sodium::cell<OptionalMediaItem> &item,
              const sodium::stream<sodium::unit> &sTogglePlayVideo,
              const sodium::stream<qint64> &sStepVideo,
              const sodium::stream<bool> &sFullscreen,
              const sodium::stream<qreal> &sScale);

    const sodium::cell<OptionalMediaItem> &item() const;

    void scaleToFit();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool event(QEvent *ev) override;

private:
    Viewer *currentViewer() const;

    sodium::cell<OptionalMediaItem> m_item;
    sodium::stream_sink<qreal> m_sPinch;
    Unsubscribe m_unsubscribe;
    QTimer m_scaleToFitTimer;
    std::unordered_map<MediaType, Viewer *> m_viewers;
    QStackedLayout *m_layout;
};
