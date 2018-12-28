#include "browserwindow.h"

#include "directorytree.h"
#include "filmrollview.h"

#include <QSplitter>

BrowserWindow::BrowserWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_fileTree(new DirectoryTree)
{
    auto splitter = new QSplitter(Qt::Horizontal);
    setCentralWidget(splitter);

    auto imageView = new FilmRollView;
    imageView->setModel(&m_model);

    splitter->addWidget(m_fileTree);
    splitter->addWidget(imageView);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    setFocusProxy(m_fileTree);

    connect(m_fileTree, &DirectoryTree::currentPathChanged, [this](const QString &path) {
        m_model.setPath(path);
    });
    m_model.setPath(m_fileTree->currentPath());
}

BrowserWindow::~BrowserWindow() {}
