#pragma once

#include "sqwidgetbase.h"

#include <QLabel>

#include <sodium/sodium.h>

class SQLabel : public SQWidgetBase<QLabel>
{
public:
    explicit SQLabel(QWidget *parent = nullptr);

    void setText(const sodium::cell<QString> &text);
    const sodium::cell<QString> &text() const;

private:
    sodium::cell<QString> m_text;
};
