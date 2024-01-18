#include "sqcheckbox.h"

using namespace sodium;

SQCheckBox::SQCheckBox(QWidget *parent)
    : SQWidgetBase<QCheckBox>(parent)
    , m_isChecked(false)
{}

void SQCheckBox::setText(const sodium::cell<QString> &text)
{
    m_unsubscribe.insert_or_assign("text",
                                   text.listen(
                                       ensureSameThread<QString>(this, &QCheckBox::setText)));
}

void SQCheckBox::setChecked(const sodium::stream<bool> &sChecked)
{
    stream_sink<bool> sUserChecked;
    connect(this, &QCheckBox::toggled, this, [sUserChecked](bool c) { sUserChecked.send(c); });
    m_isChecked = calm(sUserChecked.or_else(sChecked).hold(false));
    // use m_isChecked.sample in the potentially async listener,
    // in case a user change is posted in between
    m_unsubscribe.insert_or_assign("checked", m_isChecked.listen(post<bool>(this, [this](bool) {
        QCheckBox::setChecked(m_isChecked.sample());
    })));
}

const stream<bool> SQCheckBox::sChecked() const
{
    return m_isChecked.updates();
}

const sodium::cell<bool> &SQCheckBox::cChecked() const
{
    return m_isChecked;
}
