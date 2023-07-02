#include "tags.h"

#include <Plist.hpp>

#ifdef Q_OS_UNIX
#include <sys/xattr.h>
#endif

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

namespace Util {

QList<QString> getTags(const QString &filepath)
{
    const auto optAttrValue = qgetxattr(filepath, "com.apple.metadata:_kMDItemUserTags");
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

} // namespace Util
