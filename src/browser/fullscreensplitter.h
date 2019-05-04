#pragma once

#include <QStackedWidget>

QT_BEGIN_NAMESPACE
class QSplitter;
QT_END_NAMESPACE

class FullscreenSplitter : public QStackedWidget
{
public:
    enum Index { First, Second };

    FullscreenSplitter(QWidget *parent = nullptr);

    void setOrientation(Qt::Orientation orientation);
    void setWidget(Index index, QWidget *widget);

    void setFullscreenIndex(Index index);
    void setFullscreen(bool fullscreen);

    void setFullscreenChangedAction(std::function<void(bool)> action);

private:
    QSplitter *m_splitter;
    QWidget *m_first;
    QWidget *m_second;
    QWidget *m_fullscreen;
    std::function<void(bool)> m_fullscreenChangedAction;
    int m_fullscreenIndex = 0;
    bool m_isFullscreen = false;
};
