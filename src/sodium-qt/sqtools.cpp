#include "sqtools.h"

Unsubscribe &Unsubscribe::operator+=(const std::function<void()> &&unsub)
{
    m_unsubs.push_back(std::move(unsub));
    return *this;
}

Unsubscribe::~Unsubscribe()
{
    for (const auto &unsub : m_unsubs) {
        if (unsub)
            unsub();
    }
}

void post(QObject *guard, const std::function<void()> &action)

{
    QMetaObject::invokeMethod(
        guard, [action] { action(); }, Qt::QueuedConnection);
}
