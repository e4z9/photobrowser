#pragma once

#include "sqtools.h"

#include <QResizeEvent>
#include <QWidget>

#include <sodium/sodium.h>

template<class Base>
class SQWidgetBase : public Base
{
public:
    explicit SQWidgetBase(QWidget *parent = nullptr);

    void visible(const sodium::cell<bool> &visible);

    void font(const sodium::cell<QFont> &font);
    const sodium::cell<QFont> &font() const { return m_font; }

    // TODO a geometry getter would need to take the setter and the actual geometry into account
    // so maybe this should take a stream instead
    void geometry(const sodium::cell<QRect> &geometry);        // for setGeometry
    const sodium::cell<QSize> &size() const { return m_size; } // actual size from resizeEvent

    void resizeEvent(QResizeEvent *ev);

protected:
    sodium::cell<QFont> m_font;
    sodium::cell_sink<QSize> m_size;
    Unsubscribe m_unsubscribe;
};

template<class Base>
SQWidgetBase<Base>::SQWidgetBase(QWidget *parent)
    : Base{parent}
    , m_font(Base::font())
    , m_size(QSize())
{}

template<class Base>
void SQWidgetBase<Base>::visible(const sodium::cell<bool> &visible)
{
    // TODO
    // This could lead to issues if `visible` is called multiple times.
    // A way around that could be a another simple unsubscribe helper,
    // that is a std::unique_ptr holding an unsubscribe function, and
    // that calls that unsubscribe on destruction.
    // and possibly replace the current Unsubscribe by a unordered_map<string, SingleUnsubscribe>.
    // And then get rid of the multitude of huge constructors.
    m_unsubscribe += visible.listen(ensureSameThread<bool>(this, &QWidget::setVisible));
}

template<class Base>
void SQWidgetBase<Base>::font(const sodium::cell<QFont> &font)
{
    m_font = font;
    m_unsubscribe += font.listen(ensureSameThread<QFont>(this, &QWidget::setFont));
}

template<class Base>
void SQWidgetBase<Base>::geometry(const sodium::cell<QRect> &geometry)
{
    m_unsubscribe += geometry.listen(ensureSameThread<QRect>(this, &QWidget::setGeometry));
}

template<class Base>
void SQWidgetBase<Base>::resizeEvent(QResizeEvent *ev)
{
    post(this, [this, size = ev->size()] { m_size.send(size); });
}
