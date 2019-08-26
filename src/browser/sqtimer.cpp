#include "sqtimer.h"

#include "tools.h"

SQTimer::SQTimer(const sodium::stream<sodium::unit> &sTrigger)
{
    connect(this, &QTimer::timeout, this, [this] { m_sTimeout.send({}); });
    m_unsubscribe = sTrigger.listen(
        ensureSameThread<sodium::unit>(this, [this](sodium::unit) { start(); }));
}

SQTimer::~SQTimer()
{
    m_unsubscribe();
}

const sodium::stream<sodium::unit> &SQTimer::sTimeout() const
{
    return m_sTimeout;
}
