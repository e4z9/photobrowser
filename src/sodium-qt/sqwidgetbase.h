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

    void setVisible(const sodium::cell<bool> &visible);
    void setEnabled(const sodium::cell<bool> &enabled);

    void setFont(const sodium::cell<QFont> &font);
    const sodium::cell<QFont> &font() const { return m_font; }

    // TODO a setGeometry getter would need to take the setter and the actual setGeometry into account
    // so maybe this should take a stream instead
    void setGeometry(const sodium::cell<QRect> &geometry);
    const sodium::cell<QSize> &size() const { return m_size; } // actual size from resizeEvent

protected:
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
void SQWidgetBase<Base>::setVisible(const sodium::cell<bool> &visible)
{
    m_unsubscribe.insert_or_assign("visible",
                                   visible.listen(ensureSameThread<bool>(this, &Base::setVisible)));
}

template<class Base>
void SQWidgetBase<Base>::setEnabled(const sodium::cell<bool> &enabled)
{
    m_unsubscribe.insert_or_assign("enabled",
                                   enabled.listen(ensureSameThread<bool>(this, &Base::setEnabled)));
}

template<class Base>
void SQWidgetBase<Base>::setFont(const sodium::cell<QFont> &font)
{
    m_font = font;
    m_unsubscribe.insert_or_assign("font",
                                   font.listen(ensureSameThread<QFont>(this, &Base::setFont)));
}

template<class Base>
void SQWidgetBase<Base>::setGeometry(const sodium::cell<QRect> &geometry)
{
    m_unsubscribe.insert_or_assign("geometry",
                                   geometry.listen(
                                       ensureSameThread<QRect>(this, &Base::setGeometry)));
}

template<class Base>
void SQWidgetBase<Base>::resizeEvent(QResizeEvent *ev)
{
    post(this, [this, size = ev->size()] { m_size.send(size); });
}
