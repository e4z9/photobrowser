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
    const sodium::cell<bool> &isChecked() const;

    const sodium::stream<sodium::unit> &triggered() const;

private:
    sodium::stream_sink<sodium::unit> m_triggered;
    UserValue<bool> m_isChecked;
    Unsubscribe m_unsubscribe;
};
