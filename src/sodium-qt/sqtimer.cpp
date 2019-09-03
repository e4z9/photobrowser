#include "sqtimer.h"

SQTimer::SQTimer(const sodium::stream<sodium::unit> &sTrigger)
{
    connect(this, &QTimer::timeout, this, [this] { m_sTimeout.send({}); });
    m_unsubscribe += sTrigger.listen(
        ensureSameThread<sodium::unit>(this, [this](sodium::unit) { start(); }));
}

const sodium::stream<sodium::unit> &SQTimer::sTimeout() const
{
    return m_sTimeout;
}
