#pragma once

#include <QString>

namespace Util {

QString resolveSymlinks(const QString &filePath);
void moveToTrash(const QStringList &filePaths);

} // namespace Util
