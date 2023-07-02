#pragma once

#include "mediadirectorymodel.h"

#include <sqtimer.h>
#include <sqtools.h>

#include <QWidget>

#include <sodium/sodium.h>

#include <optional>
#include <unordered_map>

QT_BEGIN_NAMESPACE
class QStackedLayout;
QT_END_NAMESPACE

class Viewer;

void paintDuration(QPainter *painter,
                   const QRect &rect,
                   const QFont &font,
                   const QPalette &palette,
                   const QString &str);

class ImageView : public QWidget
{
public:
    ImageView(const sodium::cell<OptionalMediaItem> &item,
              const sodium::stream<sodium::unit> &sTogglePlayVideo,
              const sodium::stream<qint64> &sStepVideo,
              const sodium::stream<bool> &sFullscreen,
              const sodium::stream<std::optional<qreal>> &sScale);

    const sodium::cell<OptionalMediaItem> &item() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool event(QEvent *ev) override;

private:
    sodium::cell<OptionalMediaItem> m_item;
    sodium::stream_sink<std::optional<qreal>> m_sPinch;
    sodium::stream_sink<sodium::unit> m_sFitAfterResizeRequest;
    sodium::cell_loop<bool> m_isFittingInView;
    Unsubscribe m_unsubscribe;
    std::unique_ptr<SQTimer> m_scaleToFitTimer;
    QStackedLayout *m_layout;
};
