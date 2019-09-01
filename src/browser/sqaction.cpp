#include "sqaction.h"

using namespace sodium;

SQAction::SQAction(const sodium::cell<QString> &text, QObject *parent)
    : SQAction(text, true, parent)
{}

SQAction::SQAction(const cell<QString> &text, const sodium::cell<bool> &enabled, QObject *parent)
    : QAction(parent)
{
    m_unsubscribe += text.listen(ensureSameThread<QString>(this, &QAction::setText));
    m_unsubscribe += enabled.listen(ensureSameThread<bool>(this, &QAction::setEnabled));
    connect(this, &QAction::triggered, this, [this]() { m_sTriggered.send({}); });
}

const sodium::stream<unit> &SQAction::sTriggered() const
{
    return m_sTriggered;
}
