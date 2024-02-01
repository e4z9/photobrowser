#include "browserwindow.h"

#include "directorytree.h"
#include "filmrollview.h"
#include "fullscreensplitter.h"
#include "sqaction.h"
#include "sqcheckbox.h"
#include "sqlineedit.h"

#include <util/fileutil.h>

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QDesktopServices>
#include <QEvent>
#include <QLabel>
#include <QMenuBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindowStateChangeEvent>

#include <sodium/sodium.h>

using namespace sodium;

const char kGeometry[] = "Geometry";
const char kWindowState[] = "WindowState";
const char kSortKey[] = "SortKey";
const char kRootPath[] = "RootPath";
const char kCurrentPath[] = "CurrentPath";
const char kIncludeSubFolders[] = "IncludeSubFolders";
const char kVideosOnly[] = "VideosOnly";

Settings::Setting::Setting(const QByteArray &key, const cell<QVariant> &value)
    : key(key)
    , value(value)
{}

const stream<QVariant> Settings::add(const QByteArray &key, const cell<QVariant> &value)
{
    Setting setting(key, value);
    m_settings.push_back(setting);
    return setting.sRestore;
}

void Settings::restore(QSettings *s)
{
    if (!s)
        return;
    for (const auto &setting : m_settings) {
        if (s->contains(setting.key))
            setting.sRestore.send(s->value(setting.key));
    }
}

void Settings::save(QSettings *s)
{
    if (!s)
        return;
    for (const auto &setting : m_settings)
        s->setValue(setting.key, setting.value.sample());
}

QMenu *BrowserWindow::createFileMenu(const cell<OptionalMediaItem> &currentItem,
                                     const cell<boost::optional<int>> &currentIndex)
{
    auto fileMenu = new QMenu(BrowserWindow::tr("File"));

    const cell<bool> anyItemSelected = currentItem.map(&isMediaItem);
    const cell<bool> videoItemSelected = currentItem.map(
        [](const OptionalMediaItem &i) { return i && i->type == MediaType::Video; });
    const auto snapshotItemFilePath = [&currentItem](const stream<unit> &s) {
        return s.snapshot(currentItem)
            .filter(&OptionalMediaItem::operator bool)
            .map([](const OptionalMediaItem &i) { return i->filePath; });
    };

    auto revealInFinder = new SQAction(BrowserWindow::tr("Reveal in Finder"),
                                       fileMenu);
    revealInFinder->setEnabled(anyItemSelected);
    revealInFinder->setShortcut({"o"});
    const stream<QString> sReveal = snapshotItemFilePath(revealInFinder->triggered());
    m_unsubscribe.insert_or_assign("reveil",
                                   sReveal.listen(post<QString>(this, &Util::revealInFinder)));

    auto openInDefaultEditor = new SQAction(BrowserWindow::tr("Open in Default Editor"),
                                            fileMenu);
    openInDefaultEditor->setEnabled(anyItemSelected);
    openInDefaultEditor->setShortcut({"ctrl+o"});
    const stream<QUrl> sOpenEditor = snapshotItemFilePath(openInDefaultEditor->triggered())
                                         .map(&QUrl::fromLocalFile);
    m_unsubscribe.insert_or_assign("openeditor",
                                   sOpenEditor.listen(post<QUrl>(this, &QDesktopServices::openUrl)));

    auto moveToTrash = new SQAction(tr("Move to Trash"), fileMenu);
    moveToTrash->setEnabled(anyItemSelected);
    moveToTrash->setShortcuts({{"Delete"}, {"Backspace"}});
    const stream<int> sMoveToTrash = moveToTrash->triggered()
                                         .snapshot(currentIndex)
                                         .filter(&boost::optional<int>::operator bool)
                                         .map([](const boost::optional<int> &i) { return *i; });
    m_unsubscribe.insert_or_assign("movetotrash",
                                   sMoveToTrash.listen(
                                       post<int>(m_model.get(),
                                                 &MediaDirectoryModel::moveItemAtIndexToTrash)));

    fileMenu->addAction(revealInFinder);
    fileMenu->addAction(openInDefaultEditor);
    fileMenu->addSeparator();
    fileMenu->addAction(moveToTrash);
    return fileMenu;
}

struct SortMenu
{
    QMenu *menu;
    cell<MediaDirectoryModel::SortKey> cSortKey;
};

static SortMenu createSortMenu(stream<MediaDirectoryModel::SortKey> sRestoreSortKey)
{
    auto sortMenu = new QMenu(BrowserWindow::tr("Sort"));

    stream_loop<bool> sSortExifChecked;
    auto sortExif = new SQAction(BrowserWindow::tr("Exif/Creation Date"),
                                 sortMenu);
    sortExif->setChecked(sSortExifChecked, true);
    const auto sSortExif = sortExif->triggered().map_to(MediaDirectoryModel::SortKey::ExifCreation);

    stream_loop<bool> sSortFileNameChecked;
    auto sortFileName = new SQAction(BrowserWindow::tr("File Name"),
                                     sortMenu);
    sortFileName->setChecked(sSortFileNameChecked, false);
    const auto sSortFileName = sortFileName->triggered().map_to(
        MediaDirectoryModel::SortKey::FileName);

    stream_loop<bool> sSortRandomChecked;
    auto sortRandom = new SQAction(BrowserWindow::tr("Random"), sortMenu);
    sortRandom->setChecked(sSortRandomChecked, false);
    const auto sSortRandom = sortRandom->triggered().map_to(MediaDirectoryModel::SortKey::Random);

    auto sortKeyGroup = new QActionGroup(sortMenu);
    for (auto action : std::vector<QAction *>{sortExif, sortFileName, sortRandom})
        sortKeyGroup->addAction(action);

    const auto cSortKey = sRestoreSortKey.or_else(sSortExif)
                              .or_else(sSortFileName)
                              .or_else(sSortRandom)
                              .hold(MediaDirectoryModel::SortKey::ExifCreation);
    sSortExifChecked.loop(sRestoreSortKey.map(
        [](auto k) { return k == MediaDirectoryModel::SortKey::ExifCreation; }));
    sSortFileNameChecked.loop(
        sRestoreSortKey.map([](auto k) { return k == MediaDirectoryModel::SortKey::FileName; }));
    sSortRandomChecked.loop(
        sRestoreSortKey.map([](auto k) { return k == MediaDirectoryModel::SortKey::Random; }));

    sortMenu->addAction(sortExif);
    sortMenu->addAction(sortFileName);
    sortMenu->addAction(sortRandom);
    return {sortMenu, cSortKey};
}

static stream<std::optional<qreal>> addScaleItems(QMenu *viewMenu)
{
    auto zoomIn = new SQAction(BrowserWindow::tr("Zoom In"), viewMenu);
    zoomIn->setShortcut({"+"});
    const auto sZoomIn = zoomIn->triggered().map([](unit) -> std::optional<qreal> { return 1.1; });

    auto zoomOut = new SQAction(BrowserWindow::tr("Zoom Out"), viewMenu);
    zoomOut->setShortcut({"-"});
    const auto sZoomOut = zoomOut->triggered().map(
        [](unit) -> std::optional<qreal> { return 0.9; });

    auto scaleToFit = new SQAction(BrowserWindow::tr("Scale to Fit"), viewMenu);
    scaleToFit->setShortcut({"="});
    const auto sScaleToFit = scaleToFit->triggered().map(
        [](unit) -> std::optional<qreal> { return {}; });

    viewMenu->addAction(zoomIn);
    viewMenu->addAction(zoomOut);
    viewMenu->addAction(scaleToFit);

    return sZoomIn.or_else(sZoomOut).or_else(sScaleToFit);
}

static stream<boost::optional<int>> addNavigationItems(
    const cell<boost::optional<int>> &currentIndex, QMenu *viewMenu)
{
    const auto stepIndex = [](int step) {
        return [step](const boost::optional<int> &i) -> boost::optional<int> {
            return i ? (*i + step) : 0;
        };
    };
    auto previousItem = new SQAction(BrowserWindow::tr("Previous"), viewMenu);
    previousItem->setShortcut({"Left"});
    const stream<boost::optional<int>> sPrevious
        = previousItem->triggered().snapshot(currentIndex).map(stepIndex(-1));

    auto nextItem = new SQAction(BrowserWindow::tr("Next"), viewMenu);
    nextItem->setShortcut({"Right"});
    const stream<boost::optional<int>> sNext
        = nextItem->triggered().snapshot(currentIndex).map(stepIndex(+1));

    viewMenu->addAction(previousItem);
    viewMenu->addAction(nextItem);

    return sPrevious.or_else(sNext);
}

struct VideoMenu
{
    QMenu *menu;
    stream<unit> sPlayStop;
    stream<qint64> sStep;
};

static VideoMenu createVideoMenu(const cell<bool> &videoItemSelected, QWidget *parent)
{
    // video actions
    auto videoMenu = new QMenu(BrowserWindow::tr("Video"), parent);

    auto playStop = new SQAction(BrowserWindow::tr("Play/Pause"), videoMenu);
    playStop->setEnabled(videoItemSelected);
    playStop->setShortcut({"Space"});

    auto stepForward = new SQAction(BrowserWindow::tr("Step Forward"), videoMenu);
    stepForward->setEnabled(videoItemSelected);
    stepForward->setShortcut({"."});
    const stream<qint64> sForward = stepForward->triggered().map(
        [](unit) { return qint64(10000); });

    auto stepBackward = new SQAction(BrowserWindow::tr("Step Backward"),
                                     videoMenu);
    stepBackward->setEnabled(videoItemSelected);
    stepBackward->setShortcut({","});
    const stream<qint64> sBackward = stepBackward->triggered().map(
        [](unit) { return qint64(-10000); });

    auto smallStepForward = new SQAction(BrowserWindow::tr("Small Step Forward"),
                                         videoMenu);
    smallStepForward->setEnabled(videoItemSelected);
    smallStepForward->setShortcut({"L"});
    const stream<qint64> sSmallForward = smallStepForward->triggered().map(
        [](unit) { return qint64(1000); });

    auto smallStepBackward = new SQAction(BrowserWindow::tr("Small Step Backward"),
                                          videoMenu);
    smallStepBackward->setEnabled(videoItemSelected);
    smallStepBackward->setShortcut({"K"});
    const stream<qint64> sSmallBackward = smallStepBackward->triggered().map(
        [](unit) { return qint64(-1000); });

    videoMenu->addAction(playStop);
    videoMenu->addAction(stepForward);
    videoMenu->addAction(stepBackward);
    videoMenu->addAction(smallStepForward);
    videoMenu->addAction(smallStepBackward);

    return {videoMenu,
            playStop->triggered(),
            sForward.or_else(sBackward).or_else(sSmallForward).or_else(sSmallBackward)};
}

class FileTreeView : public SQWidgetBase<QWidget>
{
public:
    FileTreeView(Settings &settings, QWidget *parent = nullptr);

    SQAction *recursiveAction() { return m_recursiveAction; }
    SQAction *videosOnlyAction() { return m_videosOnlyAction; }
    SQAction *searchAction() { return m_searchAction; }
    const cell<QString> &path() { return m_path; }
    const cell<QString> &filterString() { return m_filterString; }

private:
    SQAction *m_recursiveAction = nullptr;
    SQAction *m_videosOnlyAction = nullptr;
    SQAction *m_searchAction = nullptr;
    cell<QString> m_path;
    cell<QString> m_filterString;
    Unsubscribe m_unsubscribe;
};

FileTreeView::FileTreeView(Settings &settings, QWidget *parent)
    : SQWidgetBase<QWidget>(parent)
    , m_path(QString())
    , m_filterString(QString())
{
    const QString recursiveText = tr("Include Subfolders");
    const QString videosOnlyText = tr("Videos Only");

    DirectoryTree *tree = new DirectoryTree;
    stream_loop<QString> sRootPath;
    stream_loop<QString> sPath;
    tree->setRootPath(sRootPath);
    tree->setPath(sPath);
    const auto sRootPathSettings = settings.add(kRootPath, tree->rootPath());
    const auto sPathSettings = settings.add(kCurrentPath, tree->path());
    sRootPath.loop(sRootPathSettings);
    sPath.loop(sPathSettings);
    m_path = tree->path();

    SQLineEdit *filter = new SQLineEdit;
    filter->setClearButtonEnabled(true);
    m_filterString = filter->text();

    stream_loop<bool> sIsRecursive;
    auto recursiveCheckBox = new SQCheckBox(recursiveText);
    recursiveCheckBox->setChecked(sIsRecursive, false);
    m_recursiveAction = new SQAction(recursiveText, this);
    m_recursiveAction->setChecked(recursiveCheckBox->isChecked().updates(), false);
    const auto sRestoreRecursive = settings.add(kIncludeSubFolders, m_recursiveAction->isChecked());
    sIsRecursive.loop(sRestoreRecursive.or_else(m_recursiveAction->isChecked().updates()));

    stream_loop<bool> sVideosOnly;
    auto videosOnlyCheckbox = new SQCheckBox(videosOnlyText);
    videosOnlyCheckbox->setChecked(sVideosOnly, false);
    m_videosOnlyAction = new SQAction(videosOnlyText, this);
    m_videosOnlyAction->setChecked(videosOnlyCheckbox->isChecked().updates(), false);
    const auto sRestoreVideosOnly = settings.add(kVideosOnly, m_videosOnlyAction->isChecked());
    sVideosOnly.loop(sRestoreVideosOnly.or_else(m_videosOnlyAction->isChecked().updates()));

    m_searchAction = new SQAction(tr("Search"), this);
    m_searchAction->setShortcut({"Ctrl+F"});
    m_unsubscribe.insert_or_assign("filtertriggered",
                                   m_searchAction->triggered().listen(
                                       post<unit>(filter, [filter](unit) {
                                           filter->setFocus(Qt::OtherFocusReason);
                                           filter->selectAll();
                                       })));

    auto treeLayout = new QVBoxLayout;
    treeLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(treeLayout);

    auto bottomLeftWidget = new QWidget;
    auto bottomLeftLayout = new QVBoxLayout;
    bottomLeftWidget->setLayout(bottomLeftLayout);
    auto filterLayout = new QHBoxLayout;
    filterLayout->addWidget(new QLabel(tr("Search:")));
    filterLayout->addWidget(filter);
    bottomLeftLayout->addLayout(filterLayout);
    bottomLeftLayout->addWidget(recursiveCheckBox);
    bottomLeftLayout->addWidget(videosOnlyCheckbox);

    treeLayout->addWidget(tree, 10);
    treeLayout->addWidget(bottomLeftWidget);
}

BrowserWindow::BrowserWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_splitter(new FullscreenSplitter(m_sFullscreen))
{
    setCentralWidget(m_splitter);
    m_splitter->setOrientation(Qt::Horizontal);

    transaction t; // ensure single transaction

    auto tree = new FileTreeView(m_settings);

    cell_loop<MediaDirectoryModel::SortKey> cSortKey;
    m_model = std::make_unique<MediaDirectoryModel>();
    m_model->setPath(tree->path());
    m_model->setRecursive(tree->recursiveAction()->isChecked());
    m_model->setFilterString(tree->filterString());
    m_model->setVideosOnly(tree->videosOnlyAction()->isChecked());
    m_model->setSortKey(cSortKey);

    stream_loop<boost::optional<int>> sCurrentIndex;
    stream_loop<unit> sTogglePlayVideo;
    stream_loop<qint64> sStepVideo;
    stream_loop<std::optional<qreal>> sScale;
    auto imageView = new FilmRollView(sCurrentIndex,
                                      sTogglePlayVideo,
                                      sStepVideo,
                                      m_sFullscreen,
                                      sScale);
    imageView->setModel(m_model.get());

    m_splitter->setWidget(FullscreenSplitter::First, tree);
    m_splitter->setWidget(FullscreenSplitter::Second, imageView);
    m_splitter->setFullscreenIndex(FullscreenSplitter::Second);

    setFocusProxy(tree);

    m_progressTimer = std::make_unique<SQTimer>(m_model->sLoadingStarted(),
                                                m_model->sLoadingFinished());
    m_progressTimer->setInterval(50);
    m_progressTimer->setSingleShot(true);
    auto progressIndicator = new SProgressIndicator;
    progressIndicator->setVisible(m_progressTimer->timedOut()
                                      .map_to(true)
                                      .or_else(m_model->sLoadingFinished().map_to(false))
                                      .hold(false));
    progressIndicator->setGeometry(
        tree->geometry()
            .map([progressIndicator](const QRect &r) {
                const QSize sh = progressIndicator->sizeHint();
                QStyle *st = progressIndicator->style();
                const QSize bottomRightMargin{st->pixelMetric(QStyle::PM_LayoutRightMargin),
                                              st->pixelMetric(QStyle::PM_LayoutBottomMargin)};
                const QRect coordinateRect = QRect({0, 0}, r.size() - bottomRightMargin);
                QRect progressGeom = QRect({0, 0}, sh);
                progressGeom.moveBottomRight(coordinateRect.bottomRight());
                return progressGeom;
            })
            .updates());
    progressIndicator->setParent(tree);

    auto menubar = new QMenuBar(this);
    setMenuBar(menubar);

    // window title
    const cell<QString> title = imageView->currentItem().map(
        [](const OptionalMediaItem &i) { return i ? i->windowTitle() : QString(); });
    m_unsubscribe.insert_or_assign("title",
                                   title.listen(
                                       ensureSameThread<QString>(this, &QWidget::setWindowTitle)));

    // file actions
    menubar->addMenu(createFileMenu(imageView->currentItem(), imageView->currentIndex()));

    // view actions
    auto viewMenu = menubar->addMenu(
        tr("Show")); // using "view" adds stupid other actions automatically

    viewMenu->addAction(tree->recursiveAction());
    viewMenu->addAction(tree->videosOnlyAction());

    stream_loop<MediaDirectoryModel::SortKey> sRestoreSortKey;
    const auto sortMenu = createSortMenu(sRestoreSortKey);
    cSortKey.loop(sortMenu.cSortKey);
    sRestoreSortKey.loop(m_settings.addInt(kSortKey, cSortKey));
    viewMenu->addMenu(sortMenu.menu);

    viewMenu->addAction(tree->searchAction());

    viewMenu->addSeparator();

    sScale.loop(addScaleItems(viewMenu));

    viewMenu->addSeparator();

    sCurrentIndex.loop(addNavigationItems(imageView->currentIndex(), viewMenu));

    viewMenu->addSeparator();

    const cell<bool> fullscreen = m_sFullscreen.hold(false);
    const cell<QString> fullscreenText = fullscreen.map([](bool b) {
        return b ? QObject::tr("Exit Full Screen") : QObject::tr("Enter Full Screen");
    });
    auto toggleFullscreen = new SQAction(fullscreenText, viewMenu);
    toggleFullscreen->setMenuRole(QAction::NoRole);
    toggleFullscreen->setShortcut({"Meta+Ctrl+F"});
    // event filter will send new value to m_sFullscreen, so do post
    m_unsubscribe.insert_or_assign("togglefullscreentriggered",
                                   toggleFullscreen->triggered().listen(
                                       post<unit>(this, [this](unit) {
                                           if (window()->isFullScreen()) {
                                               window()->setWindowState(m_previousWindowState
                                                                        & ~Qt::WindowFullScreen);
                                           } else {
                                               window()->setWindowState(window()->windowState()
                                                                        | Qt::WindowFullScreen);
                                           }
                                       })));

    viewMenu->addAction(toggleFullscreen);

    const cell<bool> videoItemSelected = imageView->currentItem().map(
        [](const OptionalMediaItem &i) { return i && i->type == MediaType::Video; });
    const auto videoMenu = createVideoMenu(videoItemSelected, menubar);
    menubar->addMenu(videoMenu.menu);
    sTogglePlayVideo.loop(videoMenu.sPlayStop);
    sStepVideo.loop(videoMenu.sStep);

    window()->installEventFilter(this);
}

void BrowserWindow::restore(QSettings *settings)
{
    if (!settings)
        return;
    restoreGeometry(settings->value(kGeometry).toByteArray());
    restoreState(settings->value(kWindowState).toByteArray());
    m_settings.restore(settings);
}

void BrowserWindow::save(QSettings *settings)
{
    if (!settings)
        return;

    if (window()->isFullScreen())
        window()->setWindowState(windowState() & ~Qt::WindowFullScreen);
    settings->setValue(kGeometry, saveGeometry());
    settings->setValue(kWindowState, saveState());
    m_settings.save(settings);
}

bool BrowserWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == window() && event->type() == QEvent::WindowStateChange) {
        QWindowStateChangeEvent *e = static_cast<QWindowStateChangeEvent *>(event);
        m_previousWindowState = e->oldState();
        if ((m_previousWindowState & Qt::WindowFullScreen)
            != (window()->windowState() & Qt::WindowFullScreen)) {
            post(this, [this] { m_sFullscreen.send(isFullScreen()); });
        }
    }
    return QWidget::eventFilter(watched, event);
}
