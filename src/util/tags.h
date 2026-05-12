#pragma once

#include <QList>
#include <QString>

namespace Util {

// retrieves kMDItemUserTags
QList<QString> getTags(const QString &filePath);
// sets kMDItemUserTags
bool setTags(const QString &filePath, const QList<QString> &tags);

} // namespace Util
