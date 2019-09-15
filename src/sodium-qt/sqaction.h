#pragma once

#include "sqtools.h"

#include <QAction>

#include <sodium/sodium.h>

class SQAction : public QAction
{
public:
    SQAction(const sodium::cell<QString> &text, QObject *parent = nullptr);
    SQAction(const sodium::cell<QString> &text,
             const sodium::cell<bool> &enabled,
             QObject *parent = nullptr);
    SQAction(const sodium::cell<QString> &text,
             const sodium::stream<bool> &sChecked,
             const sodium::cell<bool> &enabled,
             QObject *parent = nullptr);

    const sodium::stream<sodium::unit> &sTriggered() const;

    const sodium::stream<bool> sChecked() const;
    const sodium::cell<bool> &cChecked() const;

private:
    sodium::stream_sink<sodium::unit> m_sTriggered;
    sodium::cell<bool> m_isChecked;
    Unsubscribe m_unsubscribe;
};
