#pragma once

#include <QWidget>

#include <sodium/sodium.h>

#include <sqtools.h>

QT_BEGIN_NAMESPACE
class QComboBox;
class QFileSystemModel;
class QTreeView;
QT_END_NAMESPACE

class SQAction;

class DirectoryTree : public QWidget
{
    Q_OBJECT
public:
    explicit DirectoryTree(QWidget *parent = nullptr);

    void setRootPath(const sodium::stream<QString> &rootPath);
    const sodium::cell<QString> &rootPath() const;

    void setPath(const sodium::stream<QString> &path);
    const sodium::cell<QString> &path() const;

private:
    void setRootPath(const QString &path);
    QString path(const QModelIndex &index) const;

    QComboBox *m_dirSelector;
    QTreeView *m_dirTree;
    QFileSystemModel *m_dirModel;
    UserValue<QString> m_rootPath;
    UserValue<QString> m_path;
    SQAction *m_rootPathUpAction = nullptr;
    Unsubscribe m_unsubscribe;
};
