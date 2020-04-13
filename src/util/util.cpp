#include "util.h"

namespace Util {

#ifndef Q_OS_MACOS

class ScreenSleepBlockerPrivate
{};

ScreenSleepBlocker::ScreenSleepBlocker(const QString &) {}

ScreenSleepBlocker::~ScreenSleepBlocker() {}

void block() {}
void unblock() {}

#endif // ifndef Q_OS_MACOS

} // namespace Util
