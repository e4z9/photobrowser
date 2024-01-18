#pragma once

#include "sqtools.h"

#include <QResizeEvent>
#include <QWidget>

#include <sodium/sodium.h>

template<class Base>
class SQWidgetBase : public Base
{
public:
    SQWidgetBase();
    explicit SQWidgetBase(QWidget *parent);

    void visible(const sodium::cell<bool> &visible);
    void enabled(const sodium::cell<bool> &enabled);

    void font(const sodium::cell<QFont> &font);
    const sodium::cell<QFont> &font() const { return m_font; }

    // TODO a geometry getter would need to take the setter and the actual geometry into account
    // so maybe this should take a stream instead
    void geometry(const sodium::cell<QRect> &geometry);        // for setGeometry
    const sodium::cell<QSize> &size() const { return m_size; } // actual size from resizeEvent

    void resizeEvent(QResizeEvent *ev) override;

protected:
    sodium::cell<QFont> m_font;
    sodium::cell_sink<QSize> m_size;
    Unsubscribe m_unsubscribe;
};

// ---------------------- IMPLEMENTATION ----------------------

template<class Base>
SQWidgetBase<Base>::SQWidgetBase()
    : m_font(Base::font())
    , m_size(QSize())
{}

template<class Base>
SQWidgetBase<Base>::SQWidgetBase(QWidget *parent)
    : Base{parent}
    , m_font(Base::font())
    , m_size(QSize())
{}

template<class Base>
void SQWidgetBase<Base>::visible(const sodium::cell<bool> &visible)
{
    m_unsubscribe.insert_or_assign("visible",
                                   visible.listen(
                                       ensureSameThread<bool>(this, &QWidget::setVisible)));
}

template<class Base>
void SQWidgetBase<Base>::enabled(const sodium::cell<bool> &enabled)
{
    m_unsubscribe.insert_or_assign("enabled",
                                   enabled.listen(
                                       ensureSameThread<bool>(this, &QWidget::setEnabled)));
}

template<class Base>
void SQWidgetBase<Base>::font(const sodium::cell<QFont> &font)
{
    m_font = font;
    m_unsubscribe.insert_or_assign("font",
                                   font.listen(ensureSameThread<QFont>(this, &QWidget::setFont)));
}

template<class Base>
void SQWidgetBase<Base>::geometry(const sodium::cell<QRect> &geometry)
{
    m_unsubscribe.insert_or_assign("geometry",
                                   geometry.listen(
                                       ensureSameThread<QRect>(this, &QWidget::setGeometry)));
}

template<class Base>
void SQWidgetBase<Base>::resizeEvent(QResizeEvent *ev)
{
    post(this, [this, size = ev->size()] { m_size.send(size); });
}
