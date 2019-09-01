#pragma once

#include "tools.h"

#include <QStackedWidget>

#include <sodium/sodium.h>

QT_BEGIN_NAMESPACE
class QSplitter;
QT_END_NAMESPACE

class FullscreenSplitter : public QStackedWidget
{
public:
    enum Index { First, Second };

    FullscreenSplitter(const sodium::stream<bool> &sFullscreen);

    void setOrientation(Qt::Orientation orientation);
    void setWidget(Index index, QWidget *widget);

    void setFullscreenIndex(Index index);

private:
    void setFullscreen(bool fullscreen);
    QSplitter *m_splitter;
    QWidget *m_first;
    QWidget *m_second;
    QWidget *m_fullscreen;
    Unsubscribe m_unsubscribe;
    int m_fullscreenIndex = 0;
    bool m_isFullscreen = false;
};
