#include "fileutil.h"

#include <QDir>
#include <QFileInfo>

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

} // namespace Util
