#include "sqaction.h"

using namespace sodium;

SQAction::SQAction(QObject *parent)
    : SQAction(QString(), parent)
{}

SQAction::SQAction(const sodium::cell<QString> &text, QObject *parent)
    : QAction(parent)
    , m_isChecked(false, this, [this](bool v) { QAction::setChecked(v); })
{
    connect(this, &QAction::triggered, this, [this] { m_sTriggered.send({}); });
    connect(this, &QAction::toggled, this, [this](bool c) { m_isChecked.setUserValue(c); });
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
    m_isChecked.setValue(checked, initialState);
}

const sodium::stream<unit> &SQAction::sTriggered() const
{
    return m_sTriggered;
}

const stream<bool> SQAction::sChecked() const
{
    return m_isChecked.value().updates();
}

const sodium::cell<bool> &SQAction::cChecked() const
{
    return m_isChecked.value();
}
