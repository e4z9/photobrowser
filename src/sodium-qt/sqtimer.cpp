#include "sqtimer.h"

SQTimer::SQTimer(const sodium::stream<sodium::unit> &sStart)
    : SQTimer(sStart, {})
{}

SQTimer::SQTimer(const sodium::stream<sodium::unit> &sStart,
                 const sodium::stream<sodium::unit> &sStop)
{
    connect(this, &QTimer::timeout, this, [this] { m_sTimeout.send({}); });
    m_unsubscribe += sStart.listen(
        ensureSameThread<sodium::unit>(this, [this](sodium::unit) { start(); }));
    m_unsubscribe += sStop.listen(
        ensureSameThread<sodium::unit>(this, [this](sodium::unit) { stop(); }));
}

const sodium::stream<sodium::unit> &SQTimer::sTimeout() const
{
    return m_sTimeout;
}
