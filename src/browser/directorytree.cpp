#include "directorytree.h"

#include "sqaction.h"

#include <QComboBox>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPalette>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <QDebug>

using namespace sodium;

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
    , m_rootPath(defaultRootPath(), this, [this](const QString &s) { setRootPath(s); })
    , m_path(QString(), this, [this](const QString &s) {
        m_dirTree->setCurrentIndex(m_dirModel->index(s));
    })
{
    auto vLayout = new QVBoxLayout;
    vLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(vLayout);

    styleDirModel(m_dirModel);
    m_dirTree->setModel(m_dirModel);
    styleDirTree(m_dirTree);

    auto rootPathUpButton = new QToolButton;
    rootPathUpButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_rootPathUpAction = new SQAction(tr(".."), rootPathUpButton);
    rootPathUpButton->setDefaultAction(m_rootPathUpAction);
    setRootPath(defaultRootPath());

    auto rootDirLayout = new QHBoxLayout;
    rootDirLayout->setContentsMargins(0, 0, 0, 0);

    rootDirLayout->addWidget(m_dirSelector, 10);
    rootDirLayout->addWidget(rootPathUpButton);
    vLayout->addLayout(rootDirLayout);
    vLayout->addWidget(m_dirTree);
    setFocusProxy(m_dirTree);

    connect(m_dirTree, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        m_rootPath.setUserValue(m_dirModel->filePath(index));
    });
    connect(m_dirSelector, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        m_rootPath.setUserValue(m_dirSelector->itemData(index).toString());
    });
    connect(m_dirTree->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            post<QModelIndex>(this, [this](QModelIndex) {
                m_path.setUserValue(path(m_dirTree->currentIndex()));
            }));
}

void DirectoryTree::setRootPath(const sodium::stream<QString> &rootPath)
{
    cell_loop<QString> path;
    const auto rootPathUpClicked = m_rootPathUpAction->triggered()
                                       .snapshot(path, [](unit, const QString &root) {
                                           QDir dir(root);
                                           dir.cdUp();
                                           qDebug() << root << dir.path();
                                           return dir.path();
                                       });
    m_rootPath.setValue(rootPath.or_else(rootPathUpClicked), defaultRootPath());
    path.loop(m_rootPath.value());
}

const sodium::cell<QString> &DirectoryTree::rootPath() const
{
    return m_rootPath.value();
}

void DirectoryTree::setPath(const sodium::stream<QString> &path)
{
    m_path.setValue(path, QString());
}

const sodium::cell<QString> &DirectoryTree::path() const
{
    return m_path.value();
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

QString DirectoryTree::path(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};
    return m_dirModel->filePath(index);
}
