#pragma once

#include <QString>

namespace Util {

QString resolveSymlinks(const QString &filePath);
void moveToTrash(const QStringList &filePaths);
void revealInFinder(const QString &filePath);

} // namespace Util
