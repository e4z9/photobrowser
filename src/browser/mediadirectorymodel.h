#pragma once

#include "../util/metadatautil.h"

#include <QAbstractItemModel>
#include <QDateTime>
#include <QFuture>
#include <QSize>

#include <optional.h>

class MediaItem
{
public:
    QString fileName;
    QString filePath;
    QString resolvedFilePath;
    QDateTime created;
    QDateTime lastModified;
    std::optional<Util::MetaData> metaData;
};

using MediaItems = std::vector<MediaItem>;

class MediaDirectoryModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum class Role {
        Item = Qt::UserRole
    };

    MediaDirectoryModel();

    void setPath(const QString &path);

signals:
    void loadingStarted();
    void loadingFinished();

public:
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    MediaItems m_items;
    QFuture<MediaItems> m_future;
};

Q_DECLARE_METATYPE(MediaItem)
