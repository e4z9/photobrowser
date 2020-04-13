#include "util.h"

#import <IOKit/pwr_mgt/IOPMLib.h>

namespace Util {

class ScreenSleepBlockerPrivate
{
public:
    CFStringRef reason;
    IOPMAssertionID assertionID = 0;
    bool isBlocking = false;
};

ScreenSleepBlocker::ScreenSleepBlocker(const QString &reason)
    : d(new ScreenSleepBlockerPrivate)
{
    d->reason = reason.toCFString();
}

ScreenSleepBlocker::~ScreenSleepBlocker()
{
    if (d->isBlocking)
        unblock();
    CFRelease(d->reason);
    delete d;
}

void ScreenSleepBlocker::block()
{
    if (!d->isBlocking && IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep,
                                                      kIOPMAssertionLevelOn,
                                                      d->reason,
                                                      &d->assertionID) == kIOReturnSuccess) {
        d->isBlocking = true;
    }
}

void ScreenSleepBlocker::unblock()
{
    if (d->isBlocking)
        IOPMAssertionRelease(d->assertionID);
    d->isBlocking = false;
}

} // namespace Utils
