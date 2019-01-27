#include "directorytree.h"

#include <QComboBox>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QPalette>
#include <QTreeView>
#include <QVBoxLayout>

static QString defaultRootPath()
{
    return QDir::homePath();
}

static void styleDirModel(QFileSystemModel *model)
{
    model->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
}

static void styleDirTree(QTreeView *tree)
{
    // fix background color
    tree->setFrameShape(QFrame::NoFrame);
    auto pal = tree->palette();
    pal.setColor(QPalette::Base, pal.color(QPalette::Window));
    tree->setPalette(pal);

    // show only first column
    const int columnCount = tree->header()->count();
    for (int i = 1; i < columnCount; ++i)
        tree->setColumnHidden(i, true);

    tree->setHeaderHidden(true);
    tree->setTextElideMode(Qt::ElideNone);
    tree->setExpandsOnDoubleClick(false);
}

DirectoryTree::DirectoryTree(QWidget *parent)
    : QWidget(parent)
    , m_dirSelector(new QComboBox)
    , m_dirTree(new QTreeView)
    , m_dirModel(new QFileSystemModel(this))
{
    auto vLayout = new QVBoxLayout;
    vLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(vLayout);

    styleDirModel(m_dirModel);
    m_dirTree->setModel(m_dirModel);
    styleDirTree(m_dirTree);

    layout()->addWidget(m_dirSelector);
    layout()->addWidget(m_dirTree);
    setFocusProxy(m_dirTree);

    connect(m_dirTree, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        setRootPath(m_dirModel->filePath(index));
    });

    connect(m_dirSelector, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        setRootPath(m_dirSelector->itemData(index).toString());
    });

    connect(m_dirTree->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            [this](const QModelIndex &current) {
                emit currentPathChanged(path(current));
            });

    setRootPath(defaultRootPath());
}

void DirectoryTree::setRootPath(const QString &path)
{
    const auto rootIndex = m_dirModel->setRootPath(path);
    m_dirTree->setRootIndex(rootIndex);
    m_dirSelector->clear();
    QDir dir(path);
    do {
        const QString path = dir.absolutePath();
        m_dirSelector->addItem(path, path);
    } while (dir.cdUp());
#ifdef Q_OS_MACOS
    const QString volumes = "/Volumes";
    if (QFileInfo(volumes).isDir())
        m_dirSelector->addItem(volumes, volumes);
#endif
    if (!m_dirTree->currentIndex().isValid())
        m_dirTree->setCurrentIndex(rootIndex);
}

QString DirectoryTree::rootPath() const
{
    return path(m_dirTree->rootIndex());
}

void DirectoryTree::setCurrentPath(const QString &path)
{
    m_dirTree->setCurrentIndex(m_dirModel->index(path));
}

QString DirectoryTree::path(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};
    return m_dirModel->filePath(index);
}

QString DirectoryTree::currentPath() const
{
    return path(m_dirTree->currentIndex());
}
