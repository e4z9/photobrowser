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

    const sodium::stream<sodium::unit> &sTriggered() const;

private:
    sodium::stream_sink<sodium::unit> m_sTriggered;
    Unsubscribe m_unsubscribe;
};
