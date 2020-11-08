#include "metadatautil.h"

#include <QPainter>

#include <exiv2/exiv2.hpp>

static std::optional<QDateTime> extractExifCreationDateTime(const Exiv2::ExifData &exifData)
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

static std::optional<QPixmap> extractExifThumbnail(const Exiv2::ExifData &exifData,
                                                   const QSize &imageDimensions,
                                                   Util::Orientation orientation)
{
    if (exifData.empty())
        return {};
    Exiv2::ExifThumbC thumb(exifData);
    std::string ext = thumb.extension();
    if (ext.empty())
        return {};
    const Exiv2::DataBuf &data = thumb.copy();
    QPixmap pixmap;
    // cut thumbnail to original's aspect ratio, some cameras do weird things
    if (pixmap.loadFromData(data.pData_, data.size_)) {
        const auto rotatedPixmap = pixmap.transformed(
            Util::matrixForOrientation(pixmap.size(), orientation).toTransform());
        if (rotatedPixmap.size().width() == 0 || rotatedPixmap.height() == 0
            || imageDimensions.width() == 0 || imageDimensions.height() == 0) {
            return {rotatedPixmap};
        }
        // potentially cut left/right
        const int widthFromHeight = rotatedPixmap.height() * imageDimensions.width()
                                    / imageDimensions.height();
        const int xoffset = widthFromHeight < rotatedPixmap.width()
                                ? (rotatedPixmap.width() - widthFromHeight) / 2
                                : 0;
        const int targetWidth = widthFromHeight < rotatedPixmap.width() ? widthFromHeight
                                                                        : rotatedPixmap.width();
        // potentially cut top/bottom
        const int heightFromWidth = rotatedPixmap.width() * imageDimensions.height()
                                    / imageDimensions.width();
        const int yoffset = heightFromWidth < rotatedPixmap.height()
                                ? (rotatedPixmap.height() - heightFromWidth) / 2
                                : 0;
        const int targetHeight = heightFromWidth < rotatedPixmap.height() ? heightFromWidth
                                                                          : rotatedPixmap.height();
        if (xoffset == 0 && yoffset == 0)
            return {rotatedPixmap};
        QPixmap cutPixmap(targetWidth, targetHeight);
        QPainter painter(&cutPixmap);
        painter.drawPixmap(QPoint(0, 0),
                           rotatedPixmap,
                           QRect(xoffset, yoffset, targetWidth, targetHeight));
        return {cutPixmap};
    }
    return {};
}

static Util::Orientation extractExifOrientation(const Exiv2::ExifData &exifData)
{
    if (exifData.empty())
        return Util::Orientation::Normal;
    const Exiv2::ExifKey key("Exif.Image.Orientation");
    const auto md = exifData.findKey(key);
    if (md != exifData.end() && md->typeId() == Exiv2::unsignedShort)
        return Util::Orientation(md->toLong());
    return Util::Orientation::Normal;
}

static std::optional<QSize> extractExifPixelDimensions(const Exiv2::ExifData &exifData)
{
    if (exifData.empty())
        return {};
    const Exiv2::ExifKey xkey("Exif.Photo.PixelXDimension");
    const auto xmd = exifData.findKey(xkey);
    const Exiv2::ExifKey ykey("Exif.Photo.PixelYDimension");
    const auto ymd = exifData.findKey(ykey);
    if (xmd != exifData.end() && xmd->typeId() == Exiv2::unsignedLong && ymd != exifData.end()
        && ymd->typeId() == Exiv2::unsignedLong)
        return QSize(xmd->toLong(), ymd->toLong());
    return {};
}

static std::optional<QDateTime> extractXmpDateTime(const Exiv2::XmpData &data)
{
    if (data.empty())
        return {};
    const Exiv2::XmpKey key("Xmp.video.DateUTC");
    const auto md = data.findKey(key);
    if (md != data.end() && md->typeId() == Exiv2::xmpText) {
        bool ok;
        const qint64 secs = QString::fromStdString(md->toString()).toLongLong(&ok);
        if (ok && secs > 0) {
            const QDateTime baseDt({1904, 1, 1}, QTime(), Qt::UTC);
            return baseDt.addSecs(secs).toLocalTime();
        }
    }
    return {};
}

static std::optional<QSize> extractXmpDimensions(const Exiv2::XmpData &data)
{
    if (data.empty())
        return {};
    const Exiv2::XmpKey xkey("Xmp.video.Width");
    const auto xmd = data.findKey(xkey);
    const Exiv2::XmpKey ykey("Xmp.video.Height");
    const auto ymd = data.findKey(ykey);
    if (xmd != data.end() && xmd->typeId() == Exiv2::xmpText && ymd != data.end()
        && ymd->typeId() == Exiv2::xmpText) {
        bool xok;
        const int width = QString::fromStdString(xmd->toString()).toInt(&xok);
        bool yok;
        const int height = QString::fromStdString(ymd->toString()).toInt(&yok);
        if (xok && yok)
            return QSize(width, height);
    }
    return {};
}

static std::optional<qint64> extractXmpDuration(const Exiv2::XmpData &data)
{
    if (data.empty())
        return {};
    const Exiv2::XmpKey key("Xmp.video.Duration");
    const auto md = data.findKey(key);
    if (md != data.end() && md->typeId() == Exiv2::xmpText) {
        bool ok;
        const qint64 msecs = QString::fromStdString(md->toString()).toLongLong(&ok);
        if (ok)
            return msecs;
    }
    return {};
}

static QSize dimensions(const QSize &imageSize, Util::Orientation orientation)
{
    if (int(orientation) > 4)
        return {imageSize.height(), imageSize.width()};
    return imageSize;
}

namespace Util {

QMatrix4x4 matrixForOrientation(const QSize &size, Util::Orientation orientation)
{
    // matrix to get from orientation back to normal
    const auto maxx = float(size.width() - 1);
    const auto maxy = float(size.height() - 1);
    switch (orientation) {
    case Util::Orientation::Normal:
        return {};
    case Util::Orientation::FlippedHorizontal:
        return {-1, 0, 0, maxx, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    case Util::Orientation::Rotated180:
        return {-1, 0, 0, maxx, 0, -1, 0, maxy, 0, 0, 1, 0, 0, 0, 0, 1};
    case Util::Orientation::FlippedVertical:
        return {1, 0, 0, 0, 0, -1, 0, maxy, 0, 0, 1, 0, 0, 0, 0, 1};
    case Util::Orientation::RotatedClockwiseFlippedHorizontal:
        return {0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    case Util::Orientation::RotatedAntiClockwise:
        return {0, -1, 0, maxx, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    case Util::Orientation::RotatedClockwiseFlippedVertical:
        return {0, -1, 0, maxx, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    case Util::Orientation::RotatedClockwise:
        return {0, 1, 0, 0, -1, 0, 0, maxy, 0, 0, 1, 0, 0, 0, 0, 1};
    }
    return {};
}

MetaData metaData(const QString &filePath)
{
    MetaData data;
    try {
        auto image = Exiv2::ImageFactory::open(filePath.toStdString());
        image->readMetadata();

        // check exif data
        const Exiv2::ExifData &exifData = image->exifData();
        data.created = extractExifCreationDateTime(exifData);
        data.orientation = extractExifOrientation(exifData);
        data.dimensions = extractExifPixelDimensions(exifData);

        // check xmp data
        const Exiv2::XmpData &xmpData = image->xmpData();
        if (!data.created)
            data.created = extractXmpDateTime(xmpData);
        if (!data.dimensions)
            data.dimensions = extractXmpDimensions(xmpData);
        data.duration = extractXmpDuration(xmpData);

        if (!data.dimensions && image->pixelWidth() != 0 && image->pixelHeight() != 0)
            data.dimensions = QSize(image->pixelWidth(), image->pixelHeight());
        if (data.dimensions)
            data.dimensions = dimensions(*data.dimensions, data.orientation);
        data.thumbnail = extractExifThumbnail(exifData,
                                              data.dimensions ? *data.dimensions : QSize(),
                                              data.orientation);
        return data;
    } catch (...) {
    }
    return data;
}

} // namespace Util
