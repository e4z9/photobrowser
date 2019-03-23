#include "browserwindow.h"

#include "directorytree.h"
#include "filmrollview.h"

#include "../util/fileutil.h"

#include <qtc/progressindicator.h>

#include <QAction>
#include <QCheckBox>
#include <QDesktopServices>
#include <QEvent>
#include <QMenuBar>
#include <QSplitter>
#include <QVBoxLayout>

BrowserWindow::BrowserWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_fileTree(new DirectoryTree)
    , m_recursive(new QCheckBox(tr("Include Subfolders")))
{
    auto splitter = new QSplitter(Qt::Horizontal);
    setCentralWidget(splitter);

    auto imageView = new FilmRollView;
    imageView->setModel(&m_model);

    auto leftWidget = new QWidget;
    auto leftLayout = new QVBoxLayout;
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftWidget->setLayout(leftLayout);

    auto bottomLeftWidget = new QWidget;
    bottomLeftWidget->setLayout(new QVBoxLayout);
    bottomLeftWidget->layout()->addWidget(m_recursive);

    leftLayout->addWidget(m_fileTree, 10);
    leftLayout->addWidget(bottomLeftWidget);

    splitter->addWidget(leftWidget);
    splitter->addWidget(imageView);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    setFocusProxy(m_fileTree);

    connect(m_fileTree, &DirectoryTree::currentPathChanged, this, [this](const QString &path) {
        m_model.setPath(path, m_recursive->isChecked());
    });
    connect(m_recursive, &QCheckBox::toggled, this, [this](const bool checked) {
        m_model.setPath(m_fileTree->currentPath(), checked);
    });
    m_model.setPath(m_fileTree->currentPath(), m_recursive->isChecked());

    m_progressIndicator = new Utils::ProgressIndicator(Utils::ProgressIndicatorSize::Small,
                                                       leftWidget);
    leftWidget->installEventFilter(this);
    adaptProgressIndicator();
    m_progressTimer.setInterval(50);
    m_progressTimer.setSingleShot(true);
    connect(&m_progressTimer, &QTimer::timeout, m_progressIndicator, &QWidget::show);
    connect(&m_model,
            &MediaDirectoryModel::loadingStarted,
            &m_progressTimer,
            qOverload<>(&QTimer::start));
    connect(&m_model, &MediaDirectoryModel::loadingFinished, this, [this] {
        m_progressTimer.stop();
        m_progressIndicator->hide();
    });

    auto menubar = new QMenuBar(this);
    setMenuBar(menubar);

    // file actions
    auto fileMenu = menubar->addMenu(tr("File"));

    auto revealInFinder = fileMenu->addAction(tr("Reveal in Finder"));
    revealInFinder->setShortcut({"o"});
    connect(revealInFinder, &QAction::triggered, this, [this, imageView] {
        const auto item = imageView->currentItem();
        if (item)
            Util::revealInFinder(item->filePath);
    });

    auto openInDefaultEditor = fileMenu->addAction(tr("Open in Default Editor"));
    openInDefaultEditor->setShortcut({"ctrl+o"});
    connect(openInDefaultEditor, &QAction::triggered, this, [this, imageView] {
        const auto item = imageView->currentItem();
        if (item)
            QDesktopServices::openUrl(QUrl::fromLocalFile(item->filePath));
    });

    fileMenu->addSeparator();

    auto moveToTrash = fileMenu->addAction(tr("Move to Trash"));
    moveToTrash->setShortcuts({{"Delete"}, {"Backspace"}});
    connect(moveToTrash, &QAction::triggered, this, [this, imageView] {
        m_model.moveItemAtIndexToTrash(imageView->currentIndex());
    });

    connect(imageView,
            &FilmRollView::currentItemChanged,
            this,
            [imageView, moveToTrash, revealInFinder] {
                const bool hasItem = imageView->currentItem().has_value();
                revealInFinder->setEnabled(hasItem);
                moveToTrash->setEnabled(hasItem);
            });

    // view actions
    auto viewMenu = menubar->addMenu(tr("Show")); // using "view" adds stupid other actions automatically

    auto recursive = viewMenu->addAction(m_recursive->text());
    recursive->setCheckable(true);
    recursive->setChecked(m_recursive->isChecked());
    connect(m_recursive, &QCheckBox::toggled, recursive, &QAction::setChecked);
    connect(recursive, &QAction::toggled, m_recursive, &QCheckBox::setChecked);

    auto sortMenu = viewMenu->addMenu(tr("Sort"));

    m_sortExif = sortMenu->addAction(tr("Exif/Creation Date"), this, [this] {
        m_model.setSortKey(MediaDirectoryModel::SortKey::ExifCreation);
    });
    m_sortExif->setCheckable(true);
    m_sortExif->setChecked(true);

    m_sortFileName = sortMenu->addAction(tr("File Name"), this, [this] {
        m_model.setSortKey(MediaDirectoryModel::SortKey::FileName);
    });
    m_sortFileName->setCheckable(true);

    m_sortRandom = sortMenu->addAction(tr("Random"), this, [this] {
        m_model.setSortKey(MediaDirectoryModel::SortKey::Random);
    });
    m_sortRandom->setCheckable(true);

    auto sortKeyGroup = new QActionGroup(sortMenu);
    for (auto action : std::vector<QAction *>{m_sortExif, m_sortFileName, m_sortRandom})
        sortKeyGroup->addAction(action);

    viewMenu->addSeparator();

    auto zoomIn = viewMenu->addAction(tr("Zoom In"));
    zoomIn->setShortcut({"+"});
    connect(zoomIn, &QAction::triggered, imageView, &FilmRollView::zoomIn);

    auto zoomOut = viewMenu->addAction(tr("Zoom Out"));
    zoomOut->setShortcut({"-"});
    connect(zoomOut, &QAction::triggered, imageView, &FilmRollView::zoomOut);

    auto scaleToFit = viewMenu->addAction(tr("Scale to Fit"));
    scaleToFit->setShortcut({"="});
    connect(scaleToFit, &QAction::triggered, imageView, &FilmRollView::scaleToFit);

    // video actions
    auto videoMenu = menubar->addMenu(tr("Video"));

    auto playStop = videoMenu->addAction(tr("Play/Pause"));
    playStop->setShortcut({"Space"});
    connect(playStop, &QAction::triggered, imageView, &FilmRollView::togglePlayVideo);

    auto stepForward = videoMenu->addAction(tr("Step Forward"));
    stepForward->setShortcut({"."});
    connect(stepForward, &QAction::triggered, imageView, [imageView] {
        imageView->stepVideo(10000);
    });

    auto stepBackward = videoMenu->addAction(tr("Step Backward"));
    stepBackward->setShortcut({","});
    connect(stepBackward, &QAction::triggered, imageView, [imageView] {
        imageView->stepVideo(-10000);
    });

    connect(imageView,
            &FilmRollView::currentItemChanged,
            this,
            [this, imageView, playStop, stepForward, stepBackward] {
                const auto currentItem = imageView->currentItem();
                const bool enabled = (currentItem && currentItem->type == MediaType::Video);
                playStop->setEnabled(enabled);
                stepForward->setEnabled(enabled);
                stepBackward->setEnabled(enabled);
                updateWindowTitle(currentItem);
            });
}

const char kGeometry[] = "Geometry";
const char kWindowState[] = "WindowState";
const char kSortKey[] = "SortKey";
const char kRootPath[] = "RootPath";
const char kCurrentPath[] = "CurrentPath";
const char kIncludeSubFolders[] = "IncludeSubFolders";

void BrowserWindow::restore(QSettings *settings)
{
    if (!settings)
        return;
    restoreState(settings->value(kWindowState).toByteArray());
    restoreGeometry(settings->value(kGeometry).toByteArray());
    const auto sortKeyValue = settings->value(kSortKey);
    if (sortKeyValue.isValid() && sortKeyValue.canConvert<int>()) {
        const auto sortKey = MediaDirectoryModel::SortKey(sortKeyValue.toInt());
        switch (sortKey) {
        case MediaDirectoryModel::SortKey::ExifCreation:
            m_sortExif->setChecked(true);
            break;
        case MediaDirectoryModel::SortKey::FileName:
            m_sortFileName->setChecked(true);
            break;
        case MediaDirectoryModel::SortKey::Random:
            m_sortRandom->setChecked(true);
            break;
        }
        m_model.setSortKey(sortKey);
    }
    const auto rootPathValue = settings->value(kRootPath);
    if (rootPathValue.isValid())
        m_fileTree->setRootPath(rootPathValue.toString());
    const auto currentPathValue = settings->value(kCurrentPath);
    if (currentPathValue.isValid())
        m_fileTree->setCurrentPath(currentPathValue.toString());
    m_recursive->setChecked(settings->value(kIncludeSubFolders, false).toBool());
}

void BrowserWindow::save(QSettings *settings)
{
    if (!settings)
        return;
    settings->setValue(kGeometry, saveGeometry());
    settings->setValue(kWindowState, saveState());
    settings->setValue(kSortKey, int(m_model.sortKey()));
    settings->setValue(kRootPath, m_fileTree->rootPath());
    settings->setValue(kCurrentPath, m_fileTree->currentPath());
    settings->setValue(kIncludeSubFolders, m_recursive->isChecked());
}

bool BrowserWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_progressIndicator->parentWidget() && event->type() == QEvent::Resize)
        adaptProgressIndicator();
    return QWidget::eventFilter(watched, event);
}

void BrowserWindow::adaptProgressIndicator()
{
    const QSize sh = m_progressIndicator->sizeHint();
    QWidget *pp = m_progressIndicator->parentWidget();
    QStyle *st = pp->style();
    m_progressIndicator->setGeometry(
        QRect(pp->width() - sh.width() - st->pixelMetric(QStyle::PM_LayoutRightMargin),
              pp->height() - sh.height() - st->pixelMetric(QStyle::PM_LayoutBottomMargin),
              sh.width(),
              sh.height()));
}

void BrowserWindow::updateWindowTitle(const std::optional<MediaItem> &item)
{
    if (!item) {
        setWindowTitle({});
    } else {
        const QDateTime dt = item->metaData.created ? *(item->metaData.created) : item->created;
        setWindowTitle(tr("%1 - %2").arg(item->fileName, dt.toString(Qt::SystemLocaleLongDate)));
    }
}
