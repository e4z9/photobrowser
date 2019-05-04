#include "fullscreensplitter.h"

#include <QSplitter>
#include <QVBoxLayout>

FullscreenSplitter::FullscreenSplitter(QWidget *parent)
    : QStackedWidget(parent)
    , m_splitter(new QSplitter)
    , m_first(new QWidget)
    , m_second(new QWidget)
    , m_fullscreen(new QWidget)
{
    m_first->setLayout(new QVBoxLayout);
    m_first->layout()->setContentsMargins(0, 0, 0, 0);
    m_second->setLayout(new QVBoxLayout);
    m_second->layout()->setContentsMargins(0, 0, 0, 0);
    m_fullscreen->setLayout(new QVBoxLayout);
    m_fullscreen->layout()->setContentsMargins(0, 0, 0, 0);
    m_splitter->addWidget(m_first);
    m_splitter->addWidget(m_second);
    addWidget(m_splitter);
    addWidget(m_fullscreen);
}

void FullscreenSplitter::setOrientation(Qt::Orientation orientation)
{
    m_splitter->setOrientation(orientation);
}

void FullscreenSplitter::setWidget(FullscreenSplitter::Index index, QWidget *widget)
{
    m_splitter->widget(index)->layout()->addWidget(widget);
}

void FullscreenSplitter::setFullscreenIndex(Index index)
{
    m_fullscreenIndex = index;
    for (int i = 0; i < m_splitter->count(); ++i)
        m_splitter->setStretchFactor(i, i == index ? 1 : 0);
}

void FullscreenSplitter::setFullscreen(bool fullscreen)
{
    if (fullscreen == m_isFullscreen)
        return;
    m_isFullscreen = fullscreen;
    if (fullscreen) {
        m_fullscreen->layout()->addWidget(
            m_splitter->widget(m_fullscreenIndex)->layout()->itemAt(0)->widget());
        setCurrentIndex(1);
    } else {
        m_splitter->widget(m_fullscreenIndex)
            ->layout()
            ->addWidget(m_fullscreen->layout()->itemAt(0)->widget());
        setCurrentIndex(0);
    }
    if (m_fullscreenChangedAction)
        m_fullscreenChangedAction(m_isFullscreen);
}

void FullscreenSplitter::setFullscreenChangedAction(std::function<void(bool)> action)
{
    m_fullscreenChangedAction = action;
}
