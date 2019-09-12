#include "sqcheckbox.h"

using namespace sodium;

SQCheckBox::SQCheckBox(const sodium::cell<QString> &text)
    : SQCheckBox(text, {}, true)
{}

SQCheckBox::SQCheckBox(const cell<QString> &text, const sodium::cell<bool> &enabled)
    : SQCheckBox(text, {}, enabled)
{}

SQCheckBox::SQCheckBox(const sodium::cell<QString> &text,
                       const sodium::stream<bool> &sChecked,
                       const sodium::cell<bool> &enabled)
    : m_isChecked(false)
{
    m_unsubscribe += text.listen(ensureSameThread<QString>(this, &SQCheckBox::setText));
    m_unsubscribe += enabled.listen(ensureSameThread<bool>(this, &SQCheckBox::setEnabled));
    stream_sink<bool> sUserChecked;
    connect(this, &SQCheckBox::toggled, this, [sUserChecked](bool c) { sUserChecked.send(c); });
    m_isChecked = calm(sUserChecked.or_else(sChecked).hold(false));
    // use m_isChecked.sample in the potentially async listener,
    // in case a user change is posted in between
    m_unsubscribe += m_isChecked.listen(
        post<bool>(this, [this](bool) { setChecked(m_isChecked.sample()); }));
}

const sodium::cell<bool> &SQCheckBox::cChecked() const
{
    return m_isChecked;
}
