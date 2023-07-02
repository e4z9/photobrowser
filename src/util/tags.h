#pragma once

#include <QList>
#include <QString>

namespace Util {

// retrieves kMDItemUserTags
QList<QString> getTags(const QString &filePath);

} // namespace Util
