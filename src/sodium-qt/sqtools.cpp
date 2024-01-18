#include "sqtools.h"
void post(QObject *guard, const std::function<void()> &action)

{
    QMetaObject::invokeMethod(
        guard, [action] { action(); }, Qt::QueuedConnection);
}

UnsubscribeFunction::UnsubscribeFunction(const std::function<void()> &&unsub)
    : m_unsub(std::move(unsub))
{}

UnsubscribeFunction::~UnsubscribeFunction()
{
    if (m_unsub)
        m_unsub();
}

UnsubscribeFunction::UnsubscribeFunction(UnsubscribeFunction &&other)
    : m_unsub(std::move(other.m_unsub))
{
    other.m_unsub = {};
}

UnsubscribeFunction &UnsubscribeFunction::operator=(UnsubscribeFunction &&other)
{
    m_unsub = std::move(other.m_unsub);
    other.m_unsub = {};
    return *this;
}
