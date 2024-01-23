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

    void setGeometry(const sodium::stream<QRect> &geometry);
    const sodium::cell<QRect> &geometry() const { return m_geometry.value(); }

protected:
    void moveEvent(QMoveEvent *ev) override;
    void resizeEvent(QResizeEvent *ev) override;

protected:
    sodium::cell<QFont> m_font;
    UserValue<QRect> m_geometry;
    Unsubscribe m_unsubscribe;
    bool m_blockGeometryEvents = false;
    bool m_resizeEventScheduled = false;
};

// ---------------------- IMPLEMENTATION ----------------------

template<class Base>
SQWidgetBase<Base>::SQWidgetBase()
    : SQWidgetBase<Base>(nullptr)
{}

template<class Base>
SQWidgetBase<Base>::SQWidgetBase(QWidget *parent)
    : Base{parent}
    , m_font(Base::font())
    , m_geometry(QRect(), this, [this](const QRect &r) {
        if (r.isNull()) // update with null initial value, where we don't have any info
            return;
        // when we set the geometry because of sodium events, we
        // should not send another event via the move/resizeEvent
        m_blockGeometryEvents = true;
        Base::setGeometry(r);
        m_blockGeometryEvents = false;
    })
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
void SQWidgetBase<Base>::setGeometry(const sodium::stream<QRect> &geometry)
{
    m_geometry.setValue(geometry, Base::geometry());
}

template<class Base>
void SQWidgetBase<Base>::moveEvent(QMoveEvent *ev)
{
    Base::moveEvent(ev);
    if (m_blockGeometryEvents)
        return;
    // we need to use post, because move/resize events can be triggered by setVisible,
    // which can be within a sodium update
    post<QRect>(this, [this](const QRect &r) {
        if (!m_resizeEventScheduled)
            m_geometry.setUserValue(r);
    })(Base::geometry());
}

template<class Base>
void SQWidgetBase<Base>::resizeEvent(QResizeEvent *ev)
{
    Base::resizeEvent(ev);
    if (m_blockGeometryEvents)
        return;
    // we need to use post, because move/resize events can be triggered by setVisible,
    // which can be within a sodium update
    m_resizeEventScheduled = true; // compress a move event directly followed by a resize event
    post<QRect>(this, [this](const QRect &r) {
        m_resizeEventScheduled = false;
        m_geometry.setUserValue(r);
    })(Base::geometry());
}
