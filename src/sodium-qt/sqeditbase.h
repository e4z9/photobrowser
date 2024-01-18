#pragma once

#include "sqtools.h"

#include <QString>
#include <QThread>
#include <QTimer>

#include <sodium/sodium.h>

template<class Base>
class SQEditBase : public Base
{
public:
    SQEditBase(const sodium::stream<QString> &sText,
               const QString &initialText,
               const sodium::cell<bool> &enabled);
    virtual ~SQEditBase() {}

    const sodium::cell<QString> &text() const;

protected:
    const sodium::stream_sink<QString> m_sUserChanges;
    sodium::cell<QString> m_text;
    Unsubscribe m_unsubscribe;
};

template<class Base>
SQEditBase<Base>::SQEditBase(const sodium::stream<QString> &sText,
                             const QString &initialText,
                             const sodium::cell<bool> &enabled)
    : Base(initialText)
    , m_text(initialText)
{
    m_unsubscribe.insert_or_assign("enabled",
                                   enabled.listen(ensureSameThread<bool>(this, &Base::setEnabled)));

    m_text = calm(m_sUserChanges.or_else(sText).hold(initialText));
    // use m_text.sample in the potentially async listener,
    // in case a user change is posted in between
    m_unsubscribe.insert_or_assign("text", m_text.listen(post<QString>(this, [this](QString) {
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
