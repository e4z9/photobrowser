#pragma once

#include "sqwidgetbase.h"

#include <QCheckBox>

#include <sodium/sodium.h>

class SQCheckBox : public SQWidgetBase<QCheckBox>
{
public:
    explicit SQCheckBox(QWidget *parent = nullptr);
    SQCheckBox(const sodium::cell<QString> &text, QWidget *parent = nullptr);

    void setText(const sodium::cell<QString> &text);

    void setChecked(const sodium::stream<bool> &sChecked, bool initialValue);
    const sodium::cell<bool> &isChecked() const;

private:
    UserValue<bool> m_isChecked;
};
