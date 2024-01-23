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
    const stream<QString> sReveal = snapshotItemFilePath(revealInFinder->sTriggered());
    m_unsubscribe.insert_or_assign("reveil",
                                   sReveal.listen(post<QString>(this, &Util::revealInFinder)));

    auto openInDefaultEditor = new SQAction(BrowserWindow::tr("Open in Default Editor"),
                                            fileMenu);
    openInDefaultEditor->setEnabled(anyItemSelected);
    openInDefaultEditor->setShortcut({"ctrl+o"});
    const stream<QUrl> sOpenEditor = snapshotItemFilePath(openInDefaultEditor->sTriggered())
                                         .map(&QUrl::fromLocalFile);
    m_unsubscribe.insert_or_assign("openeditor",
                                   sOpenEditor.listen(post<QUrl>(this, &QDesktopServices::openUrl)));

    auto moveToTrash = new SQAction(tr("Move to Trash"), fileMenu);
    moveToTrash->setEnabled(anyItemSelected);
    moveToTrash->setShortcuts({{"Delete"}, {"Backspace"}});
    const stream<int> sMoveToTrash = moveToTrash->sTriggered()
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
    const auto sSortExif = sortExif->sTriggered().map_to(MediaDirectoryModel::SortKey::ExifCreation);

    stream_loop<bool> sSortFileNameChecked;
    auto sortFileName = new SQAction(BrowserWindow::tr("File Name"),
                                     sortMenu);
    sortFileName->setChecked(sSortFileNameChecked, false);
    const auto sSortFileName = sortFileName->sTriggered().map_to(
        MediaDirectoryModel::SortKey::FileName);

    stream_loop<bool> sSortRandomChecked;
    auto sortRandom = new SQAction(BrowserWindow::tr("Random"), sortMenu);
    sortRandom->setChecked(sSortRandomChecked, false);
    const auto sSortRandom = sortRandom->sTriggered().map_to(MediaDirectoryModel::SortKey::Random);

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
    const auto sZoomIn = zoomIn->sTriggered().map([](unit) -> std::optional<qreal> { return 1.1; });

    auto zoomOut = new SQAction(BrowserWindow::tr("Zoom Out"), viewMenu);
    zoomOut->setShortcut({"-"});
    const auto sZoomOut = zoomOut->sTriggered().map(
        [](unit) -> std::optional<qreal> { return 0.9; });

    auto scaleToFit = new SQAction(BrowserWindow::tr("Scale to Fit"), viewMenu);
    scaleToFit->setShortcut({"="});
    const auto sScaleToFit = scaleToFit->sTriggered().map(
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
        = previousItem->sTriggered().snapshot(currentIndex).map(stepIndex(-1));

    auto nextItem = new SQAction(BrowserWindow::tr("Next"), viewMenu);
    nextItem->setShortcut({"Right"});
    const stream<boost::optional<int>> sNext
        = nextItem->sTriggered().snapshot(currentIndex).map(stepIndex(+1));

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
    const stream<qint64> sForward = stepForward->sTriggered().map(
        [](unit) { return qint64(10000); });

    auto stepBackward = new SQAction(BrowserWindow::tr("Step Backward"),
                                     videoMenu);
    stepBackward->setEnabled(videoItemSelected);
    stepBackward->setShortcut({","});
    const stream<qint64> sBackward = stepBackward->sTriggered().map(
        [](unit) { return qint64(-10000); });

    auto smallStepForward = new SQAction(BrowserWindow::tr("Small Step Forward"),
                                         videoMenu);
    smallStepForward->setEnabled(videoItemSelected);
    smallStepForward->setShortcut({"L"});
    const stream<qint64> sSmallForward = smallStepForward->sTriggered().map(
        [](unit) { return qint64(1000); });

    auto smallStepBackward = new SQAction(BrowserWindow::tr("Small Step Backward"),
                                          videoMenu);
    smallStepBackward->setEnabled(videoItemSelected);
    smallStepBackward->setShortcut({"K"});
    const stream<qint64> sSmallBackward = smallStepBackward->sTriggered().map(
        [](unit) { return qint64(-1000); });

    videoMenu->addAction(playStop);
    videoMenu->addAction(stepForward);
    videoMenu->addAction(stepBackward);
    videoMenu->addAction(smallStepForward);
    videoMenu->addAction(smallStepBackward);

    return {videoMenu,
            playStop->sTriggered(),
            sForward.or_else(sBackward).or_else(sSmallForward).or_else(sSmallBackward)};
}

BrowserWindow::BrowserWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_splitter(new FullscreenSplitter(m_sFullscreen))
{
    setCentralWidget(m_splitter);
    m_splitter->setOrientation(Qt::Horizontal);
    const QString recursiveText = tr("Include Subfolders");
    const QString videosOnlyText = tr("Videos Only");

    transaction t; // ensure single transaction

    stream_loop<QString> sRootPath;
    stream_loop<QString> sPath;
    m_fileTree = new DirectoryTree(sRootPath, sPath);
    const auto sRootPathSettings = m_settings.add(kRootPath, m_fileTree->rootPath());
    const auto sPathSettings = m_settings.add(kCurrentPath, m_fileTree->path());
    sRootPath.loop(sRootPathSettings);
    sPath.loop(sPathSettings);

    auto filterInput = new SQLineEdit;
    filterInput->setClearButtonEnabled(true);
    stream_loop<bool> sIsRecursive; // loop for the action's recursive property + settings
    auto recursiveCheckBox = new SQCheckBox(recursiveText);
    recursiveCheckBox->setChecked(sIsRecursive, false);
    stream_loop<bool> sVideosOnly; // loop for the action's videosOnly property + settings
    auto videosOnlyCheckbox = new SQCheckBox(videosOnlyText);
    videosOnlyCheckbox->setChecked(sVideosOnly, false);

    const auto cIsRecursive = recursiveCheckBox->cChecked().map(&IsRecursive::fromBool);
    const auto cFilter = videosOnlyCheckbox->cChecked()
                             .lift(filterInput->text(), [](bool b, const QString &t) {
                                 return MediaDirectoryModel::Filter({t, b});
                             });
    cell_loop<MediaDirectoryModel::SortKey> cSortKey;
    m_model = std::make_unique<MediaDirectoryModel>(m_fileTree->path(),
                                                    cIsRecursive,
                                                    cFilter,
                                                    cSortKey);

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

    auto leftWidget = new QWidget;
    auto leftLayout = new QVBoxLayout;
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftWidget->setLayout(leftLayout);

    auto bottomLeftWidget = new QWidget;
    auto bottomLeftLayout = new QVBoxLayout;
    bottomLeftWidget->setLayout(bottomLeftLayout);
    auto filterLayout = new QHBoxLayout;
    filterLayout->addWidget(new QLabel(tr("Search:")));
    filterLayout->addWidget(filterInput);
    bottomLeftLayout->addLayout(filterLayout);
    bottomLeftLayout->addWidget(recursiveCheckBox);
    bottomLeftLayout->addWidget(videosOnlyCheckbox);

    leftLayout->addWidget(m_fileTree, 10);
    leftLayout->addWidget(bottomLeftWidget);

    m_splitter->setWidget(FullscreenSplitter::First, leftWidget);
    m_splitter->setWidget(FullscreenSplitter::Second, imageView);
    m_splitter->setFullscreenIndex(FullscreenSplitter::Second);

    setFocusProxy(m_fileTree);

    leftWidget->installEventFilter(this);
    m_progressTimer = std::make_unique<SQTimer>(m_model->sLoadingStarted(),
                                                m_model->sLoadingFinished());
    m_progressTimer->setInterval(50);
    m_progressTimer->setSingleShot(true);
    m_progressIndicator = new SProgressIndicator;
    m_progressIndicator->setVisible(m_progressTimer->sTimeout()
                                        .map_to(true)
                                        .or_else(m_model->sLoadingFinished().map_to(false))
                                        .hold(false));
    m_progressIndicator->setParent(leftWidget);
    adaptProgressIndicator();

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

    auto recursive = new SQAction(recursiveText, viewMenu);
    recursive->setChecked(recursiveCheckBox->sChecked(), false);
    // close the loop
    const auto sRestoreRecursive = m_settings.add(kIncludeSubFolders, recursive->cChecked());
    sIsRecursive.loop(sRestoreRecursive.or_else(recursive->sChecked()));

    auto videosOnly = new SQAction(videosOnlyText, viewMenu);
    videosOnly->setChecked(videosOnlyCheckbox->sChecked(), false);
    // close the loop
    const auto sRestoreVideosOnly = m_settings.add(kVideosOnly, videosOnly->cChecked());
    sVideosOnly.loop(sRestoreVideosOnly.or_else(videosOnly->sChecked()));

    viewMenu->addAction(recursive);
    viewMenu->addAction(videosOnly);

    stream_loop<MediaDirectoryModel::SortKey> sRestoreSortKey;
    const auto sortMenu = createSortMenu(sRestoreSortKey);
    cSortKey.loop(sortMenu.cSortKey);
    sRestoreSortKey.loop(m_settings.addInt(kSortKey, cSortKey));
    viewMenu->addMenu(sortMenu.menu);

    auto filter = new SQAction(tr("Search"), viewMenu);
    filter->setShortcut({"Ctrl+F"});
    m_unsubscribe.insert_or_assign("filtertriggered",
                                   filter->sTriggered().listen(
                                       post<unit>(filterInput, [filterInput](unit) {
                                           filterInput->setFocus(Qt::OtherFocusReason);
                                           filterInput->selectAll();
                                       })));
    viewMenu->addAction(filter);

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
                                   toggleFullscreen->sTriggered().listen(
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
    if (watched == m_progressIndicator->parentWidget() && event->type() == QEvent::Resize) {
        adaptProgressIndicator();
    } else if (watched == window() && event->type() == QEvent::WindowStateChange) {
        QWindowStateChangeEvent *e = static_cast<QWindowStateChangeEvent *>(event);
        m_previousWindowState = e->oldState();
        if ((m_previousWindowState & Qt::WindowFullScreen)
            != (window()->windowState() & Qt::WindowFullScreen)) {
            post(this, [this] { m_sFullscreen.send(isFullScreen()); });
        }
    }
    return QWidget::eventFilter(watched, event);
}

void BrowserWindow::adaptProgressIndicator()
{
    const QSize sh = m_progressIndicator->sizeHint();
    QWidget *pp = m_progressIndicator->parentWidget();
    QStyle *st = pp->style();
    m_progressIndicator->QWidget::setGeometry(
        QRect(pp->width() - sh.width() - st->pixelMetric(QStyle::PM_LayoutRightMargin),
              pp->height() - sh.height() - st->pixelMetric(QStyle::PM_LayoutBottomMargin),
              sh.width(),
              sh.height()));
}
