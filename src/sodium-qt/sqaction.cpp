#include "sqaction.h"

using namespace sodium;

SQAction::SQAction(const sodium::cell<QString> &text, QObject *parent)
    : SQAction(text, {}, true, parent)
{}

SQAction::SQAction(const cell<QString> &text, const sodium::cell<bool> &enabled, QObject *parent)
    : SQAction(text, {}, enabled, parent)
{}

SQAction::SQAction(const sodium::cell<QString> &text,
                   const sodium::stream<bool> &sChecked,
                   const sodium::cell<bool> &enabled,
                   QObject *parent)
    : QAction(parent)
    , m_isChecked(false)
{
    m_unsubscribe.insert_or_assign("text",
                                   text.listen(ensureSameThread<QString>(this, &QAction::setText)));
    m_unsubscribe.insert_or_assign("enabled",
                                   enabled.listen(
                                       ensureSameThread<bool>(this, &QAction::setEnabled)));
    connect(this, &QAction::triggered, this, [this] { m_sTriggered.send({}); });
    stream_sink<bool> sUserChecked;
    connect(this, &QAction::toggled, this, [sUserChecked](bool c) { sUserChecked.send(c); });
    m_isChecked = calm(sUserChecked.or_else(sChecked).hold(false));
    // use m_isChecked.sample in the potentially async listener,
    // in case a user change is posted in between
    m_unsubscribe.insert_or_assign("checked", m_isChecked.listen(post<bool>(this, [this](bool) {
        setChecked(m_isChecked.sample());
    })));
}

const sodium::stream<unit> &SQAction::sTriggered() const
{
    return m_sTriggered;
}

const stream<bool> SQAction::sChecked() const
{
    return m_isChecked.updates();
}

const sodium::cell<bool> &SQAction::cChecked() const
{
    return m_isChecked;
}
