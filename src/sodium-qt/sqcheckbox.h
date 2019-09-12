#pragma once

#include "sqtools.h"

#include <QCheckBox>

#include <sodium/sodium.h>

class SQCheckBox : public QCheckBox
{
public:
    SQCheckBox(const sodium::cell<QString> &text);
    SQCheckBox(const sodium::cell<QString> &text, const sodium::cell<bool> &enabled);
    SQCheckBox(const sodium::cell<QString> &text,
               const sodium::stream<bool> &sChecked,
               const sodium::cell<bool> &enabled);

    const sodium::cell<bool> &cChecked() const;

private:
    sodium::cell<bool> m_isChecked;
    Unsubscribe m_unsubscribe;
};
