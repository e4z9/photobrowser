#pragma once

#include "../util/metadatautil.h"

#include <QAbstractItemModel>
#include <QDateTime>
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

class MediaDirectoryModel : public QAbstractItemModel
{
public:
    enum class Role {
        Item = Qt::UserRole
    };

    MediaDirectoryModel();

    void setPath(const QString &path);

public:
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    std::vector<MediaItem> m_items;
};

Q_DECLARE_METATYPE(MediaItem)
