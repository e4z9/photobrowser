#pragma once

#include "sqtools.h"

#include <QAction>

#include <sodium/sodium.h>

class SQAction : public QAction
{
public:
    explicit SQAction(QObject *parent = nullptr);
    SQAction(const sodium::cell<QString> &text, QObject *parent = nullptr);

    void setText(const sodium::cell<QString> &text);
    void setEnabled(const sodium::cell<bool> &enabled);

    void setChecked(const sodium::stream<bool> &checked, bool initialState);

    const sodium::stream<sodium::unit> &sTriggered() const;

    const sodium::stream<bool> sChecked() const;
    const sodium::cell<bool> &cChecked() const;

private:
    sodium::stream_sink<sodium::unit> m_sTriggered;
    sodium::cell<bool> m_isChecked;
    Unsubscribe m_unsubscribe;
};
