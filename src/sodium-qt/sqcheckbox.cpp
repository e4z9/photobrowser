#include "sqcheckbox.h"

using namespace sodium;

SQCheckBox::SQCheckBox(QWidget *parent)
    : SQCheckBox(QString(), parent)
{}

SQCheckBox::SQCheckBox(const sodium::cell<QString> &text, QWidget *parent)
    : SQWidgetBase<QCheckBox>(parent)
    , m_isChecked(false, this, [this](bool v) { QCheckBox::setChecked(v); })
{
    connect(this, &QCheckBox::toggled, this, [this](bool c) { m_isChecked.setUserValue(c); });
    setText(text);
}

void SQCheckBox::setText(const sodium::cell<QString> &text)
{
    m_unsubscribe.insert_or_assign("text",
                                   text.listen(
                                       ensureSameThread<QString>(this, &QCheckBox::setText)));
}

void SQCheckBox::setChecked(const sodium::stream<bool> &sChecked, bool initialValue)
{
    m_isChecked.setValue(sChecked, initialValue);
}

const sodium::cell<bool> &SQCheckBox::isChecked() const
{
    return m_isChecked.value();
}
