#include "mediadirectorymodel.h"

#include "../util/fileutil.h"
#include "../util/metadatautil.h"

#include <qtc/runextensions.h>

#include <QDir>
#include <QImageReader>

#include <QDebug>

static const QDateTime &createdDateTime(const MediaItem &item)
{
    if (item.metaData && item.metaData->created)
        return *(item.metaData->created);
    if (item.created.isValid())
        return item.created;
    return item.lastModified;
}

static bool itemLessThan(const MediaItem &a, const MediaItem &b)
{
    return createdDateTime(a) < createdDateTime(b);
}

MediaDirectoryModel::MediaDirectoryModel() = default;

void MediaDirectoryModel::setPath(const QString &path)
{
    m_future.cancel();
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit loadingStarted();
    m_future = Utils::runAsync([path](QFutureInterface<MediaItems> &fi) {
        const QDir dir(path);
        const auto entryList = dir.entryInfoList(QDir::Files);
        MediaItems items;
        items.reserve(entryList.size());
        for (const auto &entry : entryList) {
            if (fi.isCanceled())
                break;
            const QString resolvedFilePath = Util::resolveSymlinks(entry.filePath());
            if (!QImageReader::imageFormat(resolvedFilePath).isEmpty()) {
                QFileInfo fi(resolvedFilePath);
                const auto metaData = Util::metaData(resolvedFilePath);
                items.push_back({entry.fileName(),
                                 entry.filePath(),
                                 resolvedFilePath,
                                 fi.birthTime(),
                                 fi.lastModified(),
                                 metaData});
            }
        }
        items.shrink_to_fit();
        std::sort(items.begin(), items.end(), itemLessThan);
        if (!fi.isCanceled())
            fi.reportResult(items);
    });
    Utils::onResultReady(m_future, this, [this](const MediaItems &items) {
        beginResetModel();
        m_items = items;
        endResetModel();
        emit loadingFinished();
        m_future = QFuture<MediaItems>();
    });
}

QModelIndex MediaDirectoryModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return {};
    return createIndex(row, column);
}

QModelIndex MediaDirectoryModel::parent(const QModelIndex &) const
{
    return {};
}

int MediaDirectoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

int MediaDirectoryModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

QVariant MediaDirectoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0
        || index.row() >= m_items.size()) {
        return {};
    }
    if (role == Qt::DisplayRole)
        return m_items.at(index.row()).fileName;
    if (role == int(Role::Item))
        return qVariantFromValue(m_items.at(index.row()));
    return {};
}
