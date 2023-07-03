#include "mediadirectorymodel.h"

#include <util/fileutil.h>

#include <qtc/runextensions.h>

#include <QDir>
#include <QDirIterator>
#include <QImageReader>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <random>

using namespace sodium;

Q_GLOBAL_STATIC(QThreadPool, sThreadPool);

namespace {

bool itemLessThanExifCreation(const MediaItem &a, const MediaItem &b)
{
    const QDateTime &da = a.createdDateTime();
    const QDateTime &db = b.createdDateTime();
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

MediaDirectoryModel::MediaDirectoryModel(const cell<QString> &path,
                                         const cell<IsRecursive> &isRecursive,
                                         const sodium::cell<Filter> &filter,
                                         const cell<SortKey> &sortKey)
    : m_path(path)
    , m_isRecursive(isRecursive)
    , m_filter(filter)
    , m_sortKey(sortKey)
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

    m_unsubscribe += m_path.listen(post<QString>(this, [this](QString) {
        load(); /*trigger reload*/
    }));
    m_unsubscribe += m_isRecursive.listen(post<IsRecursive>(this, [this](IsRecursive) {
        load(); /*trigger reload*/
    }));
    m_unsubscribe += m_filter.listen(post<Filter>(this, [this](Filter) {
        load(); /*trigger reload*/
    }));
    m_unsubscribe += m_sortKey.listen(post<SortKey>(this, [this](SortKey key) { setSortKey(key); }));
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
                               const QString &path,
                               MediaDirectoryModel::Filter filter)
{
    // scraped from https://cgit.freedesktop.org/xdg/shared-mime-info/plain/freedesktop.org.xml.in
    static QList<QByteArray> videoMimeTypes = {"video/x-flv",
                                               "application/x-matroska",
                                               "video/x-matroska",
                                               "video/webm",
                                               "application/mxf",
                                               "video/annodex",
                                               "video/ogg",
                                               "video/x-theora+ogg",
                                               "video/x-ogm+ogg",
                                               "video/mp4",
                                               "video/3gpp",
                                               "video/3gpp2",
                                               "video/vnd.rn-realvideo",
                                               "video/x-mjpeg",
                                               "video/mj2",
                                               "video/dv",
                                               "video/isivideo",
                                               "video/mpeg",
                                               "video/vnd.mpegurl",
                                               "video/quicktime",
                                               "video/vnd.vivo",
                                               "video/wavelet",
                                               "video/x-anim",
                                               "video/x-flic",
                                               "video/x-mng",
                                               "video/x-ms-wmv",
                                               "video/x-msvideo",
                                               "video/x-nsv",
                                               "video/x-sgi-movie"};
    const QList<QByteArray> supported = QImageReader::supportedMimeTypes();
    const QMimeDatabase mdb;
    const QDir dir(path);
    const auto entryList = dir.entryInfoList(QDir::Files);
    QList<std::optional<MediaItem>> optItems = QtConcurrent::blockingMapped(
        sThreadPool,
        entryList,
        [filter, &mdb, fi, supported](const QFileInfo &entry) -> std::optional<MediaItem> {
            const auto regexesFromString = [](const QString &s) {
                static const QRegularExpression whiteSpace("\\s+");
                const auto strings = s.split(whiteSpace);
                QList<QRegularExpression> result;
                std::transform(strings.cbegin(),
                               strings.cend(),
                               std::back_inserter(result),
                               [](const QString &s) {
                                   return QRegularExpression(
                                       s,
                                       QRegularExpression::CaseInsensitiveOption
                                           | QRegularExpression::MultilineOption);
                               });
                return result;
            };
            const std::optional<QList<QRegularExpression>> regexes
                = filter.searchString.isEmpty()
                      ? std::nullopt
                      : std::make_optional(regexesFromString(filter.searchString));
            const auto passesFilter = [regexes](const QList<QString> &entries) {
                if (!regexes)
                    return true;
                return std::all_of(regexes->cbegin(), regexes->cend(), [entries](const auto &rx) {
                    return std::any_of(entries.cbegin(), entries.cend(), [rx](const QString &entry) {
                        return rx.match(entry).hasMatch();
                    });
                });
            };
            if (fi.isCanceled())
                return {};
            const QString resolvedFilePath = Util::resolveSymlinks(entry.filePath());
            const auto mimeType = mdb.mimeTypeForFile(resolvedFilePath);
            if (mimeType.name() == "inode/directory")
                return {};
            MediaType type;
            if (containsMimeType(supported, mimeType)) {
                if (filter.videosOnly)
                    return {};
                type = MediaType::Image;
            } else if (containsMimeType(videoMimeTypes, mimeType)) {
                type = MediaType::Video;
            } else {
                return {};
            }
            QFileInfo fi(resolvedFilePath);
            const auto metaData = Util::metaData(resolvedFilePath);
            if (!passesFilter(metaData.tags + QList{entry.completeBaseName()}))
                return {};
            return std::make_optional<MediaItem>({entry.fileName(),
                                                  entry.filePath(),
                                                  resolvedFilePath,
                                                  fi.birthTime(),
                                                  fi.lastModified(),
                                                  std::nullopt,
                                                  metaData,
                                                  type});
        });

    optItems.erase(std::remove_if(optItems.begin(),
                                  optItems.end(),
                                  [](const std::optional<MediaItem> &i) { return !i; }),
                   optItems.end());
    MediaItems items;
    std::transform(optItems.cbegin(), optItems.cend(), std::back_inserter(items), [](const auto &i) {
        return *i;
    });
    return items;
}

void MediaDirectoryModel::load()
{
    cancelAndWait();
    const QString path = m_path.sample();
    const IsRecursive recursive = m_isRecursive.sample();
    const Filter showOption = m_filter.sample();
    const SortKey sortKey = m_sortKey.sample();
    beginResetModel();
    m_items.clear();
    endResetModel();
    m_futureWatcher.setFuture(Utils::runAsync(
        [this, sortKey, path, showOption, recursive](QFutureInterface<ResultList> &fi) {
            m_sLoadingStarted.send({});
            MediaItems results = collectItems(fi, path, showOption);
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
                    MediaItems dirResults = collectItems(fi, dir, showOption);
                    arrange(dirResults, sortKey);
                    const ResultList resultList = addArranged(sortKey, results, dirResults);
                    if (!fi.isCanceled() && !resultList.empty())
                        fi.reportResult(resultList);
                }
            }
            m_sLoadingFinished.send({});
        }));
}

void MediaDirectoryModel::moveItemAtIndexToTrash(int i)
{
    cancelAndWait();
    if (i >= int(m_items.size()))
        return;
    const QModelIndex mIndex = index(i, 0);
    beginRemoveRows(mIndex.parent(), mIndex.row(), mIndex.row());
    const MediaItem &item = m_items.at(mIndex.row());
    Util::moveToTrash({item.filePath});
    m_items.erase(std::begin(m_items) + mIndex.row());
    endRemoveRows();
}

const sodium::stream<unit> &MediaDirectoryModel::sLoadingStarted() const
{
    return m_sLoadingStarted;
}

const sodium::stream<unit> &MediaDirectoryModel::sLoadingFinished() const
{
    return m_sLoadingFinished;
}

void MediaDirectoryModel::setSortKey(SortKey key)
{
    if (m_futureWatcher.isRunning()) {
        // we need to restart the scanning
        cancelAndWait();
        load();
        return;
    }
    beginResetModel();
    if (key == SortKey::Random)
        shuffle(m_items);
    else
        std::sort(m_items.begin(), m_items.end(), itemLessThan(key));
    endResetModel();
}

bool MediaDirectoryModel::isShowingDateDisplay() const
{
    return m_sortKey.sample() == SortKey::ExifCreation;
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

static QString tagString(const Util::MetaData &metaData)
{
    QList<QString> singleLine;
    // cut off second line for colors (which have color number on second line)
    std::transform(metaData.tags.cbegin(),
                   metaData.tags.cend(),
                   std::back_inserter(singleLine),
                   [](const QString &s) { return s.left(s.indexOf('\n')); });
    return singleLine.join(", ");
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
    addRow(MediaDirectoryModel::tr("Tags:"), tagString(item.metaData));
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
    const OptionalMediaItem previousItem = index.row() > 0
                                               ? std::make_optional(m_items.at(index.row() - 1))
                                               : std::nullopt;
    if (role == Qt::DisplayRole)
        return item.fileName;
    if (role == int(Role::Item))
        return QVariant::fromValue(item);
    if (role == int(Role::Thumbnail)) {
        if (item.thumbnail)
            return *item.thumbnail;
        m_thumbnailCreator.requestThumbnail(item);
        if (item.metaData.thumbnail)
            return *item.metaData.thumbnail;
        return {};
    }
    if (role == int(Role::ShowDateDisplay))
        return isShowingDateDisplay();
    if (role == int(Role::DateDisplay)) {
        if (isShowingDateDisplay()
            && (!previousItem
                || previousItem->createdDateTime().date() != item.createdDateTime().date())) {
            return item.createdDateTime().toString("d.M.");
        }
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

const QDateTime &MediaItem::createdDateTime() const
{
    if (metaData.created) {
        if (created.isValid()) {
            // hack for devices that write local datetimes into UTC datetime metadata...
            const QDateTime createdUTC = metaData.created->toUTC();
            const QDateTime createdUTCInLocal(createdUTC.date(), createdUTC.time(), Qt::LocalTime);
            if (std::abs(createdUTCInLocal.secsTo(created)) < 5)
                return created;
        }
        return *metaData.created;
    }
    if (created.isValid())
        return created;
    return lastModified;
}

QString MediaItem::windowTitle() const
{
    const QDateTime &dt = createdDateTime();
    return QCoreApplication::translate("MediaItem", "%1%2 - %3, %4")
        .arg(fileName,
             metaData.duration ? (" - " + durationToString(*metaData.duration)) : QString(),
             dt.date().toString("ddd"),
             dt.toString(Qt::TextDate));
}

bool isMediaItem(const OptionalMediaItem &item)
{
    return bool(item);
}
