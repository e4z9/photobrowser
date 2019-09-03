#pragma once

#include "sqtools.h"

#include <QTimer>

#include <sodium/sodium.h>

class SQTimer : public QTimer
{
public:
    explicit SQTimer(const sodium::stream<sodium::unit> &sTrigger);

    const sodium::stream<sodium::unit> &sTimeout() const;

private:
    sodium::stream_sink<sodium::unit> m_sTimeout;
    Unsubscribe m_unsubscribe;
};

