#pragma once

#include <QWidget>

#include <sodium/sodium.h>

#include <sqtools.h>

QT_BEGIN_NAMESPACE
class QComboBox;
class QFileSystemModel;
class QTreeView;
QT_END_NAMESPACE

class DirectoryTree : public QWidget
{
    Q_OBJECT
public:
    explicit DirectoryTree(const sodium::stream<QString> &sRootPath, sodium::stream<QString> &sPath);

    const sodium::cell<QString> &rootPath() const;
    const sodium::cell<QString> &path() const;

private:
    void setRootPath(const QString &path);
    QString path(const QModelIndex &index) const;

    QComboBox *m_dirSelector;
    QTreeView *m_dirTree;
    QFileSystemModel *m_dirModel;
    sodium::cell<QString> m_rootPath;
    sodium::cell<QString> m_path;
    Unsubscribe m_unsubscribe;
};
