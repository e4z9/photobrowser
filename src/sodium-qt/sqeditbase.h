#pragma once

#include "sqwidgetbase.h"

#include <QString>
#include <QThread>
#include <QTimer>

#include <sodium/sodium.h>

template<class Base>
class SQEditBase : public SQWidgetBase<Base>
{
public:
    explicit SQEditBase(QWidget *parent = nullptr);
    virtual ~SQEditBase() {}

    void setText(const sodium::stream<QString> &sText, const QString &initialText = {});
    const sodium::cell<QString> &text() const;

protected:
    const sodium::stream_sink<QString> m_sUserChanges;
    sodium::cell<QString> m_text;
};

// ---------------------- IMPLEMENTATION ----------------------

template<class Base>
SQEditBase<Base>::SQEditBase(QWidget *parent)
    : SQWidgetBase<Base>(parent)
    , m_text(QString())
{
    setText({}); // set up receiving user changes
}

template<class Base>
void SQEditBase<Base>::setText(const sodium::stream<QString> &sText, const QString &initialText)
{
    m_text = calm(m_sUserChanges.or_else(sText).hold(initialText));
    // use m_text.sample in the potentially async listener,
    // in case a user change is posted in between
    SQWidgetBase<Base>::m_unsubscribe
        .insert_or_assign("text", m_text.listen(post<QString>(this, [this](QString) {
            // Setting the text changes the cursor position, so don't do it if text wouldn't change,
            // which is for example the case for user changes
            const QString text = m_text.sample();
            if (text != Base::text())
                Base::setText(m_text.sample());
        })));
}

template<class Base>
const sodium::cell<QString> &SQEditBase<Base>::text() const
{
    return m_text;
}
