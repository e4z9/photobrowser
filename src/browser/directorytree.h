#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QComboBox;
class QFileSystemModel;
class QTreeView;
QT_END_NAMESPACE

class DirectoryTree : public QWidget
{
    Q_OBJECT
public:
    explicit DirectoryTree(QWidget *parent = nullptr);

    void setRootPath(const QString &path);
    QString rootPath() const;
    void setCurrentPath(const QString &path);
    QString currentPath() const;

signals:
    void currentPathChanged(const QString &path);

private:
    QString path(const QModelIndex &index) const;

    QComboBox *m_dirSelector;
    QTreeView *m_dirTree;
    QFileSystemModel *m_dirModel;
};
