#include "mediadirectorymodel.h"

#include "../util/fileutil.h"
#include "../util/metadatautil.h"

#include <qtc/runextensions.h>

#include <QDir>
#include <QImageReader>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logGov, "browser.thumbnails", QtWarningMsg)

static void createThumbnailImage(QFutureInterface<QImage> &fi,
                                   const QString &filePath,
                                   const Util::Orientation orientation,
                                   const int maxSize)
{
    // TODO SVG and videos
    QImage image(filePath);
    if (fi.isCanceled())
        return;
    if (image.isNull()) {
        fi.reportResult(image);
        return;
    }
    image = image.transformed(Util::matrixForOrientation(image.size(), orientation));
    if (fi.isCanceled())
        return;
    if (image.width() > image.height())
        fi.reportResult(image.scaledToWidth(maxSize, Qt::SmoothTransformation));
    else
        fi.reportResult(image.scaledToHeight(maxSize, Qt::SmoothTransformation));
}

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

const int MAX_THREADS = 4;
const int MAX_PENDING = 40;

void ThumbnailGoverner::logQueueSizes() const
{
    qDebug(logGov) << "pending" << m_pending.size();
    qDebug(logGov) << "running" << m_running.size();
}

static auto findRunningItem(std::vector<ThumbnailGoverner::RunningItem> &running,
                            const QString &resolvedFilePath)
{
    return std::find_if(running.begin(),
                        running.end(),
                        [resolvedFilePath](const ThumbnailGoverner::RunningItem &item) {
                            return item.first == resolvedFilePath;
                        });
}

void ThumbnailGoverner::requestThumbnail(const MediaItem &item, bool cancelRunning)
{
    if (cancelRunning)
        cancel(item.resolvedFilePath);
    else if (findRunningItem(m_running, item.resolvedFilePath) != m_running.end())
        return;
    qDebug(logGov) << "requested" << item.resolvedFilePath;
    const Util::Orientation orientation = item.metaData ? item.metaData->orientation
                                                        : Util::Orientation::Normal;
    if (m_running.size() >= MAX_THREADS) {
        while (m_pending.size() >= 40)
            m_pending.pop();
        m_pending.push({item.resolvedFilePath, orientation});
    } else {
        start(item.resolvedFilePath, orientation);
    }
    logQueueSizes();
}

void ThumbnailGoverner::cancel(const QString &resolvedFilePath)
{
    auto runningItem = findRunningItem(m_running, resolvedFilePath);
    if (runningItem != m_running.end()) {
        qDebug(logGov) << "canceled " << runningItem->first;
        runningItem->second.cancel();
        m_running.erase(runningItem);
        logQueueSizes();
    }
}

void ThumbnailGoverner::start(const QString &resolvedFilePath, Util::Orientation orientation)
{
    auto future = Utils::runAsync(createThumbnailImage,
                                  resolvedFilePath,
                                  orientation,
                                  200);
    m_running.emplace_back<RunningItem>({resolvedFilePath, future});
    qDebug(logGov) << "started  " << resolvedFilePath;
    logQueueSizes();
    Utils::onResultReady(future, this, [this, future, resolvedFilePath](const QImage &image) {
        auto runningItem = std::find_if(m_running.begin(),
                                        m_running.end(),
                                        [future](const RunningItem &item) {
                                            return item.second == future;
                                        });
        if (runningItem != m_running.end())
            m_running.erase(runningItem);
        else
            qWarning(logGov) << "Thumbnail governer internal error: Could not find running future";
        qDebug(logGov) << "finished " << resolvedFilePath;
        logQueueSizes();
        emit thumbnailReady(resolvedFilePath, QPixmap::fromImage(image));
        startPendingItem();
    });
}

void ThumbnailGoverner::startPendingItem()
{
    if (m_pending.empty())
        return;
    const auto pending = m_pending.front();
    m_pending.pop();
    start(pending.first, pending.second);
}

MediaDirectoryModel::MediaDirectoryModel()
{
    connect(&m_thumbnailGoverner,
            &ThumbnailGoverner::thumbnailReady,
            this,
            [this](const QString &resolvedFilePath, const QPixmap &pixmap) {
                for (int i = 0; i < m_items.size(); ++i) {
                    MediaItem &item = m_items.at(i);
                    if (item.resolvedFilePath == resolvedFilePath) {
                        item.thumbnail = pixmap;
                        const QModelIndex mi = index(i, 0, QModelIndex());
                        dataChanged(mi, mi);
                    }
                }
            });
}

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
                                 std::nullopt,
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
    const MediaItem &item = m_items.at(index.row());
    if (role == Qt::DisplayRole)
        return item.fileName;
    if (role == int(Role::Item))
        return qVariantFromValue(item);
    if (role == int(Role::Thumbnail)) {
        if (item.thumbnail)
            return *item.thumbnail;
        m_thumbnailGoverner.requestThumbnail(item);
        return {};
    }
    return {};
}
