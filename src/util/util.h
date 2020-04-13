#pragma once

#include <QString>

namespace Util {

class ScreenSleepBlockerPrivate;

class ScreenSleepBlocker
{
public:
    ScreenSleepBlocker(const QString &reason);
    ~ScreenSleepBlocker();

    void block();
    void unblock();

private:
    ScreenSleepBlockerPrivate *d = nullptr;
};

} // namespace Util
