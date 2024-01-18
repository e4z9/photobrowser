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

DirectoryTree::DirectoryTree(const stream<QString> &sRootPath, sodium::stream<QString> &sPath)
    : m_dirSelector(new QComboBox)
    , m_dirTree(new QTreeView)
    , m_dirModel(new QFileSystemModel(this))
    , m_path(QString())
{
    auto vLayout = new QVBoxLayout;
    vLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(vLayout);

    styleDirModel(m_dirModel);
    m_dirTree->setModel(m_dirModel);
    styleDirTree(m_dirTree);

    auto rootPathUpButton = new QToolButton;
    rootPathUpButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto rootPathUp = new SQAction(tr(".."), rootPathUpButton);
    rootPathUpButton->setDefaultAction(rootPathUp);

    auto rootDirLayout = new QHBoxLayout;
    rootDirLayout->setContentsMargins(0, 0, 0, 0);

    rootDirLayout->addWidget(m_dirSelector, 10);
    rootDirLayout->addWidget(rootPathUpButton);
    vLayout->addLayout(rootDirLayout);
    vLayout->addWidget(m_dirTree);
    setFocusProxy(m_dirTree);

    const stream<QString> sRootPathUp = rootPathUp->sTriggered().snapshot(m_rootPath,
                                                                          [](unit,
                                                                             const QString &root) {
                                                                              QDir dir(root);
                                                                              dir.cdUp();
                                                                              return dir.path();
                                                                          });

    stream_sink<QString> sRootPathFromUI;
    connect(m_dirTree,
            &QTreeView::doubleClicked,
            this,
            [this, sRootPathFromUI](const QModelIndex &index) {
                sRootPathFromUI.send(m_dirModel->filePath(index));
            });
    connect(m_dirSelector,
            qOverload<int>(&QComboBox::activated),
            this,
            [this, sRootPathFromUI](int index) {
                sRootPathFromUI.send(m_dirSelector->itemData(index).toString());
            });
    m_rootPath.loop(sRootPathFromUI.or_else(sRootPath).or_else(sRootPathUp).hold(defaultRootPath()));
    m_unsubscribe.insert_or_assign("rootpath",
                                   m_rootPath.listen(
                                       ensureSameThread<QString>(this, [this](const QString &p) {
                                           setRootPath(p);
                                       })));

    stream_sink<QString> sPathFromTree;
    connect(m_dirTree->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            post<QModelIndex>(this, [this, sPathFromTree](QModelIndex) {
                sPathFromTree.send(path(m_dirTree->currentIndex()));
            }));
    m_path = sPathFromTree.or_else(sPath).hold(QString());
    m_unsubscribe.insert_or_assign("path",
                                   m_path.listen(
                                       ensureSameThread<QString>(this, [this](const QString &p) {
                                           m_dirTree->setCurrentIndex(m_dirModel->index(p));
                                       })));
}

const sodium::cell<QString> &DirectoryTree::rootPath() const
{
    return m_rootPath;
}

const sodium::cell<QString> &DirectoryTree::path() const
{
    return m_path;
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
