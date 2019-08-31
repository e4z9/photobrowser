#include "browserwindow.h"

#include "directorytree.h"
#include "filmrollview.h"
#include "fullscreensplitter.h"
#include "sqaction.h"

#include "../util/fileutil.h"

#include <qtc/progressindicator.h>

#include <QAction>
#include <QCheckBox>
#include <QDesktopServices>
#include <QEvent>
#include <QMenuBar>
#include <QUrl>
#include <QVBoxLayout>

#include <sodium/sodium.h>

using namespace sodium;

BrowserWindow::BrowserWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_splitter(new FullscreenSplitter)
    , m_fileTree(new DirectoryTree)
    , m_recursive(new QCheckBox(tr("Include Subfolders")))
{
    setCentralWidget(m_splitter);

    m_splitter->setOrientation(Qt::Horizontal);

    auto imageView = new FilmRollView;
    imageView->setModel(&m_model);
    m_splitter->setFullscreenChangedAction(
        [imageView](bool fullscreen) { imageView->setFullscreen(fullscreen); });

    auto leftWidget = new QWidget;
    auto leftLayout = new QVBoxLayout;
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftWidget->setLayout(leftLayout);

    auto bottomLeftWidget = new QWidget;
    bottomLeftWidget->setLayout(new QVBoxLayout);
    bottomLeftWidget->layout()->addWidget(m_recursive);

    leftLayout->addWidget(m_fileTree, 10);
    leftLayout->addWidget(bottomLeftWidget);

    m_splitter->setWidget(FullscreenSplitter::First, leftWidget);
    m_splitter->setWidget(FullscreenSplitter::Second, imageView);
    m_splitter->setFullscreenIndex(FullscreenSplitter::Second);

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

    const cell<bool> anyItemSelected = imageView->currentItem().map(
        [](const OptionalMediaItem &i) { return bool(i); });
    const auto snapshotItemFilePath = [imageView](const stream<unit> &s) {
        return s
            .snapshot(imageView->currentItem(), [](unit, const OptionalMediaItem &i) { return i; })
            .filter(&OptionalMediaItem::operator bool)
            .map([](const OptionalMediaItem &i) { return i->filePath; });
    };

    auto revealInFinder = new SQAction(tr("Reveal in Finder"), anyItemSelected, fileMenu);
    revealInFinder->setShortcut({"o"});
    const stream<QString> sReveal = snapshotItemFilePath(revealInFinder->sTriggered());
    m_unsubscribe += sReveal.listen(ensureSameThread<QString>(this, &Util::revealInFinder));

    auto openInDefaultEditor = new SQAction(tr("Open in Default Editor"), anyItemSelected, fileMenu);
    openInDefaultEditor->setShortcut({"ctrl+o"});
    const stream<QString> sOpenEditor = snapshotItemFilePath(openInDefaultEditor->sTriggered());
    m_unsubscribe += sOpenEditor.listen(ensureSameThread<QString>(this, [](const QString &fp) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fp));
    }));

    auto moveToTrash = new SQAction(tr("Move to Trash"), anyItemSelected, fileMenu);
    moveToTrash->setShortcuts({{"Delete"}, {"Backspace"}});
    const stream<int> sMoveToTrash = moveToTrash->sTriggered()
                                         .snapshot(imageView->currentIndex(),
                                                   [](unit, const auto &i) { return i; })
                                         .filter(&boost::optional<int>::operator bool)
                                         .map([](const boost::optional<int> &i) { return *i; });
    m_unsubscribe += sMoveToTrash.listen(ensureSameThread<int>(this, [this](const int &i) {
        m_model.moveItemAtIndexToTrash(m_model.index(i, 0));
    }));

    fileMenu->addAction(revealInFinder);
    fileMenu->addAction(openInDefaultEditor);
    fileMenu->addSeparator();
    fileMenu->addAction(moveToTrash);

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

    viewMenu->addSeparator();

    auto previousItem = viewMenu->addAction(tr("Previous"));
    previousItem->setShortcut({"Left"});
    connect(previousItem, &QAction::triggered, imageView, &FilmRollView::previous);

    auto nextItem = viewMenu->addAction(tr("Next"));
    nextItem->setShortcut({"Right"});
    connect(nextItem, &QAction::triggered, imageView, &FilmRollView::next);

    viewMenu->addSeparator();

    m_toggleFullscreen = viewMenu->addAction(tr("Enter Full Screen"));
    m_toggleFullscreen->setMenuRole(QAction::NoRole);
    m_toggleFullscreen->setShortcut({"Meta+Ctrl+F"});
    connect(m_toggleFullscreen, &QAction::triggered, this, [this] {
        if (window()->isFullScreen())
            window()->setWindowState(window()->windowState() & ~Qt::WindowFullScreen);
        else
            window()->setWindowState(window()->windowState() | Qt::WindowFullScreen);
    });

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
                const auto currentItem = imageView->_currentItem();
                const bool enabled = (currentItem && currentItem->type == MediaType::Video);
                playStop->setEnabled(enabled);
                stepForward->setEnabled(enabled);
                stepBackward->setEnabled(enabled);
                updateWindowTitle(currentItem);
            });

    window()->installEventFilter(this);
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
    restoreGeometry(settings->value(kGeometry).toByteArray());
    restoreState(settings->value(kWindowState).toByteArray());
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

    if (window()->isFullScreen())
        window()->setWindowState(windowState() & ~Qt::WindowFullScreen);
    settings->setValue(kGeometry, saveGeometry());
    settings->setValue(kWindowState, saveState());
    settings->setValue(kSortKey, int(m_model.sortKey()));
    settings->setValue(kRootPath, m_fileTree->rootPath());
    settings->setValue(kCurrentPath, m_fileTree->currentPath());
    settings->setValue(kIncludeSubFolders, m_recursive->isChecked());
}

bool BrowserWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_progressIndicator->parentWidget() && event->type() == QEvent::Resize) {
        adaptProgressIndicator();
    } else if (watched == window() && event->type() == QEvent::WindowStateChange) {
        if (window()->isFullScreen()) {
            m_toggleFullscreen->setText(tr("Exit Full Screen"));
        } else {
            m_toggleFullscreen->setText(tr("Enter Full Screen"));
        }
        m_splitter->setFullscreen(isFullScreen());
    }
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
