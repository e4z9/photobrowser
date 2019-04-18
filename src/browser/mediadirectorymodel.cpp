#include "mediadirectorymodel.h"

#include "../util/fileutil.h"

#include <qtc/runextensions.h>

#include <QDir>
#include <QDirIterator>
#include <QImageReader>
#include <QMimeDatabase>

#include <algorithm>
#include <random>

namespace {

const QDateTime &createdDateTime(const MediaItem &item)
{
    if (item.metaData.created)
        return *item.metaData.created;
    if (item.created.isValid())
        return item.created;
    return item.lastModified;
}

bool itemLessThanExifCreation(const MediaItem &a, const MediaItem &b)
{
    const QDateTime &da = createdDateTime(a);
    const QDateTime &db = createdDateTime(b);
    if (da == db)
        return a.resolvedFilePath < b.resolvedFilePath;
    return da < db;
}

bool itemLessThanFileName(const MediaItem &a, const MediaItem &b)
{
    return a.fileName.compare(b.fileName, Qt::CaseInsensitive) < 0;
}

std::function<bool(const MediaItem &, const MediaItem &)> itemLessThan(
    MediaDirectoryModel::SortKey key)
{
    if (key == MediaDirectoryModel::SortKey::ExifCreation)
        return itemLessThanExifCreation;
    return itemLessThanFileName;
}

template<class C>
void shuffle(C &&c)
{
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(c.begin(), c.end(), g);
}

void arrange(MediaItems &items, MediaDirectoryModel::SortKey key)
{
    if (key == MediaDirectoryModel::SortKey::Random)
        shuffle(items);
    else
        std::sort(items.begin(), items.end(), itemLessThan(key));
}

MediaDirectoryModel::ResultList addSorted(MediaDirectoryModel::SortKey key,
                                          MediaItems &target,
                                          const MediaItems &source)
{
    MediaDirectoryModel::ResultList resultList;
    target.reserve(target.size() + source.size());
    auto current = source.begin();
    const auto end = source.end();
    auto insertionPoint = target.begin();
    while (current != end) {
        while (insertionPoint != target.end() && itemLessThan(key)(*insertionPoint, *current))
            ++insertionPoint;
        auto currentEnd = insertionPoint != target.end() ? current + 1 : source.end();
        while (currentEnd != source.end() && itemLessThan(key)(*currentEnd, *insertionPoint))
            ++currentEnd;
        const auto insertionIndex = std::distance(target.begin(), insertionPoint);
        const auto insertionCount = std::distance(current, currentEnd);
        insertionPoint = target.insert(insertionPoint, current, currentEnd);
        resultList.push_back({insertionIndex, MediaItems(current, currentEnd)});
        insertionPoint += insertionCount;
        current = currentEnd;
    }
    return resultList;
}

MediaDirectoryModel::ResultList addArranged(MediaDirectoryModel::SortKey key,
                                            MediaItems &target,
                                            const MediaItems &source)
{
    if (key != MediaDirectoryModel::SortKey::Random)
        return addSorted(key, target, source);
    // random
    MediaDirectoryModel::ResultList resultList;
    target.reserve(target.size() + source.size());
    std::random_device rd;
    std::mt19937 g(rd());
    using distr_t = std::uniform_int_distribution<MediaItems::size_type>;
    using distr_param_t = distr_t::param_type;
    distr_t distribute;
    for (const MediaItem &item : source) {
        const auto insertionIndex = distribute(g, distr_param_t(0, target.size()));
        target.insert(target.begin() + insertionIndex, item);
        resultList.push_back({insertionIndex, {item}});
    }
    return resultList;
}

} // namespace

MediaDirectoryModel::MediaDirectoryModel()
{
    connect(&m_thumbnailCreator,
            &ThumbnailCreator::thumbnailReady,
            this,
            [this](const QString &resolvedFilePath,
                   const QPixmap &pixmap,
                   std::optional<qint64> duration) {
                for (int i = 0; i < m_items.size(); ++i) {
                    MediaItem &item = m_items.at(i);
                    if (item.resolvedFilePath == resolvedFilePath) {
                        item.thumbnail = pixmap;
                        if (duration)
                            item.metaData.duration = duration;
                        const QModelIndex mi = index(i, 0, QModelIndex());
                        dataChanged(mi, mi);
                    }
                }
            });
    connect(&m_futureWatcher, &QFutureWatcherBase::resultReadyAt, this, [this](int index) {
        for (const auto &value : m_futureWatcher.resultAt(index))
            insertItems(value.first, value.second);
    });
    connect(&m_futureWatcher,
            &QFutureWatcherBase::finished,
            this,
            &MediaDirectoryModel::loadingFinished);
}

MediaDirectoryModel::~MediaDirectoryModel()
{
    cancelAndWait();
}

static bool containsMimeType(const QList<QByteArray> &list, const QMimeType &type)
{
    for (const QByteArray &value : list) {
        const auto name = QString::fromUtf8(value);
        if (type.name() == name || type.aliases().contains(name) || type.inherits(name))
            return true;
    }
    return false;
}

static MediaItems collectItems(QFutureInterface<MediaDirectoryModel::ResultList> &fi,
                               const QString &path)
{
    const QDir dir(path);
    const auto entryList = dir.entryInfoList(QDir::Files);
    MediaItems items;
    items.reserve(entryList.size());
    const QList<QByteArray> supported = QImageReader::supportedMimeTypes();
    const QMimeDatabase mdb;
    for (const auto &entry : entryList) {
        if (fi.isCanceled())
            return {};
        const QString resolvedFilePath = Util::resolveSymlinks(entry.filePath());
        const auto mimeType = mdb.mimeTypeForFile(resolvedFilePath);
        if (mimeType.name() == "inode/directory")
            continue;
        MediaType type;
        if (containsMimeType(supported, mimeType))
            type = MediaType::Image;
        else if (QMediaPlayer::hasSupport(mimeType.name()))
            type = MediaType::Video;
        else
            continue;
        QFileInfo fi(resolvedFilePath);
        const auto metaData = Util::metaData(resolvedFilePath);
        items.push_back({entry.fileName(),
                         entry.filePath(),
                         resolvedFilePath,
                         fi.birthTime(),
                         fi.lastModified(),
                         std::nullopt,
                         metaData,
                         type});
    }
    return std::move(items);
}

void MediaDirectoryModel::setPath(const QString &path, bool recursive)
{
    cancelAndWait();
    m_path = path;
    m_isRecursive = recursive;
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit loadingStarted();
    m_futureWatcher.setFuture(
        Utils::runAsync([sortKey = m_sortKey, path, recursive](QFutureInterface<ResultList> &fi) {
            MediaItems results = collectItems(fi, path);
            if (fi.isCanceled())
                return;
            arrange(results, sortKey);
            if (!results.empty())
                fi.reportResult({{0, results}});
            if (recursive) {
                QDirIterator it(path,
                                QDir::Dirs | QDir::NoDotAndDotDot,
                                QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
                while (it.hasNext()) {
                    if (fi.isCanceled())
                        break;
                    const QString dir = it.next();
                    MediaItems dirResults = collectItems(fi, dir);
                    arrange(dirResults, sortKey);
                    const ResultList resultList = addArranged(sortKey, results, dirResults);
                    if (!fi.isCanceled() && !resultList.empty())
                        fi.reportResult(resultList);
                }
            }
        }));
}

void MediaDirectoryModel::moveItemAtIndexToTrash(const QModelIndex &index)
{
    cancelAndWait();
    if (!index.isValid() || index.row() >= m_items.size())
        return;
    beginRemoveRows(index.parent(), index.row(), index.row());
    const MediaItem &item = m_items.at(index.row());
    Util::moveToTrash({item.filePath});
    m_items.erase(std::begin(m_items) + index.row());
    endRemoveRows();
}

void MediaDirectoryModel::setSortKey(SortKey key)
{
    m_sortKey = key;
    if (m_futureWatcher.isRunning()) {
        // we need to restart the scanning
        cancelAndWait();
        setPath(m_path, m_isRecursive);
        return;
    }
    beginResetModel();
    if (key == SortKey::Random)
        shuffle(m_items);
    else
        std::sort(m_items.begin(), m_items.end(), itemLessThan(key));
    endResetModel();
}

MediaDirectoryModel::SortKey MediaDirectoryModel::sortKey() const
{
    return m_sortKey;
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

static QString toolTip(const MediaItem &item)
{
    static constexpr char format[] = "dd.MM.yyyy HH:mm:ss";
    QString tooltip = "<html><body>";
    const auto addRow = [&tooltip](const QString &title, const QString &value) {
        tooltip += "<tr><td style=\"padding-right: 5px\">" + title + "</td><td>" + value
                   + "</td></tr>";
    };
    const auto addEmptyRow = [&tooltip] { tooltip += "<tr/>"; };
    tooltip += "<table>";
    addRow(MediaDirectoryModel::tr("File:"), item.fileName);
    if (item.resolvedFilePath != item.filePath) {
        addEmptyRow();
        addRow(MediaDirectoryModel::tr("Original:"), item.resolvedFilePath);
    }
    addRow(MediaDirectoryModel::tr("Size:"), sizeToString(QFileInfo(item.resolvedFilePath).size()));
    addEmptyRow();
    if (item.metaData.duration)
        addRow(MediaDirectoryModel::tr("Duration:"), durationToString(*item.metaData.duration));
    if (item.metaData.dimensions) {
        addRow(MediaDirectoryModel::tr("Dimensions:"),
               MediaDirectoryModel::tr("%1 x %2")
                   .arg(item.metaData.dimensions->width())
                   .arg(item.metaData.dimensions->height()));
    }
    if (item.metaData.created)
        addRow(MediaDirectoryModel::tr("Date:"), item.metaData.created->toString(format));
    addEmptyRow();
    addRow(MediaDirectoryModel::tr("Created:"), item.created.toString(format));
    addRow(MediaDirectoryModel::tr("Modified:"), item.lastModified.toString(format));
    tooltip += "</table>";
    tooltip += "</body></html>";
    return tooltip;
}

QVariant MediaDirectoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0
        || index.row() >= m_items.size()) {
        return {};
    }
    const MediaItem &item = m_items.at(index.row());
    if (role == Qt::DisplayRole)
        return item.fileName;
    if (role == int(Role::Item))
        return qVariantFromValue(item);
    if (role == int(Role::Thumbnail)) {
        if (item.thumbnail)
            return *item.thumbnail;
        m_thumbnailCreator.requestThumbnail(item);
        if (item.metaData.thumbnail)
            return *item.metaData.thumbnail;
        return {};
    }
    if (role == Qt::ToolTipRole)
        return toolTip(item);
    return {};
}

void MediaDirectoryModel::insertItems(int index, const MediaItems &items)
{
    if (items.empty() || index > m_items.size())
        return;
    if (m_items.empty()) {
        beginResetModel();
        m_items = items;
        endResetModel();
    } else {
        beginInsertRows(QModelIndex(), index, index + items.size() - 1);
        m_items.insert(std::begin(m_items) + index, std::begin(items), std::end(items));
        endInsertRows();
    }
}

void MediaDirectoryModel::cancelAndWait()
{
    m_futureWatcher.cancel();
    m_futureWatcher.waitForFinished();
}

QString durationToString(const qint64 durationMs)
{
    QTime duration(0, 0);
    duration = duration.addMSecs(durationMs);
    const QString format = duration.hour() > 0 ? "HH:mm:ss" : "mm:ss";
    return duration.toString(format);
}

QString sizeToString(const qint64 size)
{
    if (size >= 1000000)
        return MediaDirectoryModel::tr("%1 MB").arg(QString::number(size / 1000000));
    if (size >= 1000)
        return MediaDirectoryModel::tr("%1 KB").arg(QString::number(size / 1000));
    return MediaDirectoryModel::tr("%1 B").arg(QString::number(size));
}
