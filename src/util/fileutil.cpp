#include "fileutil.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace Util {

QString resolveSymlinks(const QString &filePath)
{
    QFileInfo fi(filePath);
    int count = 0;
    static const int maxCount = 10;
    while (++count <= maxCount && fi.isSymLink())
        fi.setFile(fi.dir(), fi.symLinkTarget());
    if (count > maxCount)
        return {};
    return fi.filePath();
}

void revealInFinder(const QString &filePath)
{
    // TODO non-macOS
    QProcess::execute("/usr/bin/osascript",
                      {"-e",
                       "tell application \"Finder\"",
                       "-e",
                       "activate",
                       "-e",
                       "reveal POSIX file \"" + filePath + "\"",
                       "-e",
                       "end tell"});
}

#ifndef Q_OS_MACOS
void moveToTrash(const QStringList &filePaths)
{
    // TODO
}
#endif

} // namespace Util
