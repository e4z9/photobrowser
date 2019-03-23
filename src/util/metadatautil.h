#pragma once

#include <optional.h>

#include <QDateTime>
#include <QPixmap>
#include <QSize>

namespace Util {

enum class Orientation {
    Normal = 1,
    FlippedHorizontal = 2,
    Rotated180 = 3,
    FlippedVertical = 4,
    RotatedClockwiseFlippedHorizontal = 5,
    RotatedAntiClockwise = 6,
    RotatedClockwiseFlippedVertical = 7,
    RotatedClockwise = 8
};

class MetaData
{
public:
    std::optional<QSize> dimensions;
    std::optional<QDateTime> created;
    std::optional<QPixmap> thumbnail;
    std::optional<qint64> duration;
    Orientation orientation;
};

QMatrix matrixForOrientation(const QSize &size, Util::Orientation orientation);
MetaData metaData(const QString &filePath);

} // namespace Util
