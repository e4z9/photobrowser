#include "tags.h"

#include <Plist.hpp>

#include <QDebug>

#ifdef Q_OS_UNIX
#include <sys/xattr.h>
#endif

const char kItemUserTags[] = "com.apple.metadata:_kMDItemUserTags";

static std::optional<QByteArray> qgetxattr(const QString &filepath, const QString &attr)
{
#ifdef Q_OS_UNIX
    const auto bFilepath = filepath.toUtf8();
    const auto path = bFilepath.data();
    const auto bAttr = attr.toUtf8();
    const auto name = bAttr.data();
    const auto size = getxattr(path, name, NULL, 0, 0, 0);
    if (size < 0)
        return {};
    QByteArray ret;
    ret.resize(size);
    getxattr(path, name, ret.data(), size, 0, 0);
    return ret;
#else
    return {};
#endif
}

static int qsetxattr(const QString &filepath, const QString &attr, const QByteArray &data)
{
#ifdef Q_OS_UNIX
    const auto bFilepath = filepath.toUtf8();
    const auto path = bFilepath.data();
    const auto bAttr = attr.toUtf8();
    const auto name = bAttr.data();
    const auto value = data.data();
    const auto valueSize = data.size();
    return setxattr(path, name, data, valueSize, 0, 0);
#else
    return 45 /* ENOTSUP Operation not supported */;
#endif
}

namespace Util {

QList<QString> getTags(const QString &filepath)
{
    const auto optAttrValue = qgetxattr(filepath, kItemUserTags);
    if (!optAttrValue)
        return {};
    boost::any result;
    Plist::readPlist(optAttrValue->data(), optAttrValue->size(), result);
    try {
        const auto key = boost::any_cast<Plist::array_type>(result);
        QList<QString> ret;
        std::transform(key.cbegin(), key.cend(), std::back_inserter(ret), [](const boost::any &v) {
            const auto s = boost::any_cast<Plist::string_type>(v);
            return QString::fromStdString(s);
        });
        return ret;
    } catch (boost::bad_any_cast &) {
    }
    return {};
}

bool setTags(const QString &filePath, const QList<QString> &tags)
{
    Plist::array_type array;
    std::transform(tags.cbegin(), tags.cend(), std::back_inserter(array), [](const QString &v) {
        return v.toStdString();
    });
    std::vector<char> data;
    Plist::writePlistBinary(data, array);
    const QByteArray bData(data.data(), data.size());
    return qsetxattr(filePath, kItemUserTags, bData);
}

} // namespace Util
