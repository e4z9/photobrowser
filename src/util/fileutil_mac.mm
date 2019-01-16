#include "fileutil.h"

#include <QStringList>

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

namespace Util {

void moveToTrash(const QStringList &filePaths)
{
    NSMutableArray<NSURL *> *urls = [NSMutableArray<NSURL *> array];
    for (const auto &path : filePaths)
        [urls addObject:[NSURL fileURLWithPath:path.toNSString()]];
    [[NSWorkspace sharedWorkspace] recycleURLs:urls completionHandler:nil];
}

} // namespace Util
