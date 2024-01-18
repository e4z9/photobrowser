#pragma once

#include "sqwidgetbase.h"

#include <QCheckBox>

#include <sodium/sodium.h>

class SQCheckBox : public SQWidgetBase<QCheckBox>
{
public:
    explicit SQCheckBox(QWidget *parent = nullptr);

    void text(const sodium::cell<QString> &text);
    void checked(const sodium::stream<bool> &sChecked);

    const sodium::stream<bool> sChecked() const;
    const sodium::cell<bool> &cChecked() const;

private:
    sodium::cell<bool> m_isChecked;
};
