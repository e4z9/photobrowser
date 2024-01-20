#include "sqaction.h"

using namespace sodium;

SQAction::SQAction(QObject *parent)
    : SQAction(QString(), parent)
{}

SQAction::SQAction(const sodium::cell<QString> &text, QObject *parent)
    : QAction(parent)
    , m_isChecked(false)
{
    connect(this, &QAction::triggered, this, [this] { m_sTriggered.send({}); });
    setText(text);
}

void SQAction::setText(const sodium::cell<QString> &text)
{
    m_unsubscribe.insert_or_assign("text",
                                   text.listen(ensureSameThread<QString>(this, &QAction::setText)));
}

void SQAction::setEnabled(const sodium::cell<bool> &enabled)
{
    m_unsubscribe.insert_or_assign("enabled",
                                   enabled.listen(
                                       ensureSameThread<bool>(this, &QAction::setEnabled)));
}

void SQAction::setChecked(const sodium::stream<bool> &checked, bool initialState)
{
    setCheckable(true);
    stream_sink<bool> sUserChecked;
    connect(this, &QAction::toggled, this, [sUserChecked](bool c) { sUserChecked.send(c); });
    m_isChecked = calm(sUserChecked.or_else(checked).hold(initialState));
    // use m_isChecked.sample in the potentially async listener,
    // in case a user change is posted in between
    m_unsubscribe.insert_or_assign("checked", m_isChecked.listen(post<bool>(this, [this](bool) {
        QAction::setChecked(m_isChecked.sample());
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
