#include "metadatautil.h"

#include <exiv2/exiv2.hpp>

static std::optional<QDateTime> extractCreationDateTime(const Exiv2::ExifData &exifData)
{
    if (exifData.empty())
        return {};
    const Exiv2::ExifKey key("Exif.Photo.DateTimeOriginal");
    const auto md = exifData.findKey(key);
    if (md != exifData.end() && md->typeId() == Exiv2::asciiString) {
        const auto dateTime = QDateTime::fromString(QString::fromStdString(md->toString()),
                                                    "yyyy:MM:dd hh:mm:ss");
        return dateTime.isValid() ? std::make_optional(dateTime) : std::nullopt;
    }
    return {};
}

static std::optional<QPixmap> extractThumbnail(const Exiv2::ExifData &exifData,
                                               Util::Orientation orientation)
{
    Exiv2::ExifThumbC thumb(exifData);
    std::string ext = thumb.extension();
    if (ext.empty())
        return {};
    const Exiv2::DataBuf &data = thumb.copy();
    QPixmap pixmap;
    if (pixmap.loadFromData(data.pData_, data.size_))
        return pixmap.transformed(Util::matrixForOrientation(pixmap.size(), orientation));
    return {};
}

static Util::Orientation extractOrientation(const Exiv2::ExifData &exifData)
{
    if (exifData.empty())
        return Util::Orientation::Normal;
    const Exiv2::ExifKey key("Exif.Image.Orientation");
    const auto md = exifData.findKey(key);
    if (md != exifData.end() && md->typeId() == Exiv2::unsignedShort)
        return Util::Orientation(md->toLong());
    return Util::Orientation::Normal;
}

static QSize dimensions(const QSize &imageSize, Util::Orientation orientation)
{
    if (int(orientation) > 4)
        return {imageSize.height(), imageSize.width()};
    return imageSize;
}

namespace Util {

QMatrix matrixForOrientation(const QSize &size, Util::Orientation orientation)
{
    // matrix to get from orientation back to normal
    const auto maxx = qreal(size.width() - 1);
    const auto maxy = qreal(size.height() - 1);
    switch (orientation) {
    case Util::Orientation::Normal:
        return {};
    case Util::Orientation::FlippedHorizontal:
        return {-1, 0, 0, 1, maxx, 0};
    case Util::Orientation::Rotated180:
        return {-1, 0, 0, -1, maxx, maxy};
    case Util::Orientation::FlippedVertical:
        return {1, 0, 0, -1, 0, maxy};
    case Util::Orientation::RotatedClockwiseFlippedHorizontal:
        return {0, 1, 1, 0, 0, 0};
    case Util::Orientation::RotatedAntiClockwise:
        return {0, 1, -1, 0, maxx, 0};
    case Util::Orientation::RotatedClockwiseFlippedVertical:
        return {0, 1, -1, 0, maxx, 0};
    case Util::Orientation::RotatedClockwise:
        return {0, -1, 1, 0, 0, maxy};
    }
    return {};
}

MetaData metaData(const QString &filePath)
{
    MetaData data;
    try {
        auto image = Exiv2::ImageFactory::open(filePath.toStdString());
        image->readMetadata();
        const Exiv2::ExifData &exifData = image->exifData();
        data.created = extractCreationDateTime(exifData);
        data.orientation = extractOrientation(exifData);
        if (image->pixelWidth() != 0 && image->pixelHeight() != 0)
            data.dimensions = dimensions({image->pixelWidth(), image->pixelHeight()}, data.orientation);
        data.thumbnail = extractThumbnail(exifData, data.orientation);
        return data;
    } catch (Exiv2::Error &error) {
    }
    return data;
}

} // namespace Util
