#pragma once

#include <QTimer>

#include <sodium/sodium.h>

class SQTimer : public QTimer
{
public:
    explicit SQTimer(const sodium::stream<sodium::unit> &sTrigger);
    ~SQTimer() override;

    const sodium::stream<sodium::unit> &sTimeout() const;

private:
    sodium::stream_sink<sodium::unit> m_sTimeout;
    std::function<void()> m_unsubscribe;
};

