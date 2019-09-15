#include "browserwindow.h"

#include "directorytree.h"
#include "filmrollview.h"
#include "fullscreensplitter.h"
#include "sqaction.h"
#include "sqcheckbox.h"

#include <util/fileutil.h>

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

const sodium::stream<bool> Settings::add(const QByteArray &key, const cell<bool> &value)
{
    return add(key, value.map([](bool b) -> QVariant { return b; })).map([](const QVariant &v) {
        return v.toBool();
    });
}

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

BrowserWindow::BrowserWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_splitter(new FullscreenSplitter(m_sFullscreen))
    , m_fileTree(new DirectoryTree)
{
    setCentralWidget(m_splitter);
    m_splitter->setOrientation(Qt::Horizontal);
    const QString recursiveText = tr("Include Subfolders");
    const QString videosOnlyText = tr("Videos Only");

    transaction t; // ensure single transaction
    stream_loop<bool> sIsRecursive; // loop for the action's recursive property + settings
    auto recursiveCheckBox = new SQCheckBox(recursiveText, sIsRecursive, true);
    stream_loop<bool> sVideosOnly; // loop for the action's videosOnly property + settings
    auto videosOnlyCheckbox = new SQCheckBox(videosOnlyText, sVideosOnly, true);

    const auto cIsRecursive = recursiveCheckBox->cChecked().map(&IsRecursive::fromBool);
    const auto cVideosOnly = videosOnlyCheckbox->cChecked().map(&VideosOnly::fromBool);
    m_model = std::make_unique<MediaDirectoryModel>(cIsRecursive, cVideosOnly);

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
    bottomLeftWidget->setLayout(new QVBoxLayout);
    bottomLeftWidget->layout()->addWidget(recursiveCheckBox);
    bottomLeftWidget->layout()->addWidget(videosOnlyCheckbox);

    leftLayout->addWidget(m_fileTree, 10);
    leftLayout->addWidget(bottomLeftWidget);

    m_splitter->setWidget(FullscreenSplitter::First, leftWidget);
    m_splitter->setWidget(FullscreenSplitter::Second, imageView);
    m_splitter->setFullscreenIndex(FullscreenSplitter::Second);

    setFocusProxy(m_fileTree);

    connect(m_fileTree,
            &DirectoryTree::currentPathChanged,
            m_model.get(),
            &MediaDirectoryModel::setPath);

    m_progressIndicator = new Utils::ProgressIndicator(Utils::ProgressIndicatorSize::Small,
                                                       leftWidget);
    leftWidget->installEventFilter(this);
    adaptProgressIndicator();
    m_progressTimer.setInterval(50);
    m_progressTimer.setSingleShot(true);
    connect(&m_progressTimer, &QTimer::timeout, m_progressIndicator, &QWidget::show);
    connect(m_model.get(),
            &MediaDirectoryModel::loadingStarted,
            &m_progressTimer,
            qOverload<>(&QTimer::start));
    connect(m_model.get(), &MediaDirectoryModel::loadingFinished, this, [this] {
        m_progressTimer.stop();
        m_progressIndicator->hide();
    });

    auto menubar = new QMenuBar(this);
    setMenuBar(menubar);

    // window title
    const cell<QString> title = imageView->currentItem().map(
        [](const OptionalMediaItem &i) { return i ? i->windowTitle() : QString(); });
    m_unsubscribe += title.listen(ensureSameThread<QString>(this, &QWidget::setWindowTitle));

    // file actions
    auto fileMenu = menubar->addMenu(tr("File"));

    const cell<bool> anyItemSelected = imageView->currentItem().map(&isMediaItem);
    const cell<bool> videoItemSelected = imageView->currentItem().map(
        [](const OptionalMediaItem &i) { return i && i->type == MediaType::Video; });
    const auto snapshotItemFilePath = [imageView](const stream<unit> &s) {
        return s.snapshot(imageView->currentItem())
            .filter(&OptionalMediaItem::operator bool)
            .map([](const OptionalMediaItem &i) { return i->filePath; });
    };

    auto revealInFinder = new SQAction(tr("Reveal in Finder"), anyItemSelected, fileMenu);
    revealInFinder->setShortcut({"o"});
    const stream<QString> sReveal = snapshotItemFilePath(revealInFinder->sTriggered());
    m_unsubscribe += sReveal.listen(post<QString>(this, &Util::revealInFinder));

    auto openInDefaultEditor = new SQAction(tr("Open in Default Editor"), anyItemSelected, fileMenu);
    openInDefaultEditor->setShortcut({"ctrl+o"});
    const stream<QUrl> sOpenEditor = snapshotItemFilePath(openInDefaultEditor->sTriggered())
                                         .map(&QUrl::fromLocalFile);
    m_unsubscribe += sOpenEditor.listen(post<QUrl>(this, &QDesktopServices::openUrl));

    auto moveToTrash = new SQAction(tr("Move to Trash"), anyItemSelected, fileMenu);
    moveToTrash->setShortcuts({{"Delete"}, {"Backspace"}});
    const stream<int> sMoveToTrash = moveToTrash->sTriggered()
                                         .snapshot(imageView->currentIndex())
                                         .filter(&boost::optional<int>::operator bool)
                                         .map([](const boost::optional<int> &i) { return *i; });
    m_unsubscribe += sMoveToTrash.listen(
        post<int>(m_model.get(), &MediaDirectoryModel::moveItemAtIndexToTrash));

    fileMenu->addAction(revealInFinder);
    fileMenu->addAction(openInDefaultEditor);
    fileMenu->addSeparator();
    fileMenu->addAction(moveToTrash);

    // view actions
    auto viewMenu = menubar->addMenu(
        tr("Show")); // using "view" adds stupid other actions automatically

    auto recursive = new SQAction(recursiveText,
                                  recursiveCheckBox->cChecked().updates(),
                                  true,
                                  viewMenu);
    recursive->setCheckable(true);
    // close the loop
    const auto sRestoreRecursive = m_settings.add(kIncludeSubFolders, recursive->cChecked());
    sIsRecursive.loop(sRestoreRecursive.or_else(recursive->cChecked().updates()));

    auto videosOnly = new SQAction(videosOnlyText,
                                   videosOnlyCheckbox->cChecked().updates(),
                                   true,
                                   viewMenu);
    videosOnly->setCheckable(true);
    // close the loop
    const auto sRestoreVideosOnly = m_settings.add(kVideosOnly, videosOnly->cChecked());
    sVideosOnly.loop(sRestoreVideosOnly.or_else(videosOnly->cChecked().updates()));

    viewMenu->addAction(recursive);
    viewMenu->addAction(videosOnly);

    auto sortMenu = viewMenu->addMenu(tr("Sort"));

    m_sortExif = sortMenu->addAction(tr("Exif/Creation Date"), this, [this] {
        m_model->setSortKey(MediaDirectoryModel::SortKey::ExifCreation);
    });
    m_sortExif->setCheckable(true);
    m_sortExif->setChecked(true);

    m_sortFileName = sortMenu->addAction(tr("File Name"), this, [this] {
        m_model->setSortKey(MediaDirectoryModel::SortKey::FileName);
    });
    m_sortFileName->setCheckable(true);

    m_sortRandom = sortMenu->addAction(tr("Random"), this, [this] {
        m_model->setSortKey(MediaDirectoryModel::SortKey::Random);
    });
    m_sortRandom->setCheckable(true);

    auto sortKeyGroup = new QActionGroup(sortMenu);
    for (auto action : std::vector<QAction *>{m_sortExif, m_sortFileName, m_sortRandom})
        sortKeyGroup->addAction(action);

    viewMenu->addSeparator();

    auto zoomIn = new SQAction(tr("Zoom In"), viewMenu);
    zoomIn->setShortcut({"+"});
    const auto sZoomIn = zoomIn->sTriggered().map([](unit) -> std::optional<qreal> { return 1.1; });

    auto zoomOut = new SQAction(tr("Zoom Out"), viewMenu);
    zoomOut->setShortcut({"-"});
    const auto sZoomOut = zoomOut->sTriggered().map(
        [](unit) -> std::optional<qreal> { return 0.9; });

    auto scaleToFit = new SQAction(tr("Scale to Fit"), viewMenu);
    scaleToFit->setShortcut({"="});
    const auto sScaleToFit = scaleToFit->sTriggered().map(
        [](unit) -> std::optional<qreal> { return {}; });

    sScale.loop(sZoomIn.or_else(sZoomOut).or_else(sScaleToFit));

    viewMenu->addAction(zoomIn);
    viewMenu->addAction(zoomOut);
    viewMenu->addAction(scaleToFit);

    viewMenu->addSeparator();

    const auto stepIndex = [](int step) {
        return [step](const boost::optional<int> &i) -> boost::optional<int> {
            return i ? (*i + step) : 0;
        };
    };
    auto previousItem = new SQAction(tr("Previous"), viewMenu);
    previousItem->setShortcut({"Left"});
    const stream<boost::optional<int>> sPrevious
        = previousItem->sTriggered().snapshot(imageView->currentIndex()).map(stepIndex(-1));

    auto nextItem = new SQAction(tr("Next"), viewMenu);
    nextItem->setShortcut({"Right"});
    const stream<boost::optional<int>> sNext
        = nextItem->sTriggered().snapshot(imageView->currentIndex()).map(stepIndex(+1));

    sCurrentIndex.loop(sPrevious.or_else(sNext));

    viewMenu->addAction(previousItem);
    viewMenu->addAction(nextItem);
    viewMenu->addSeparator();

    const cell<bool> fullscreen = m_sFullscreen.hold(false);
    const cell<QString> fullscreenText = fullscreen.map([](bool b) {
        return b ? QObject::tr("Exit Full Screen") : QObject::tr("Enter Full Screen");
    });
    auto toggleFullscreen = new SQAction(fullscreenText, viewMenu);
    toggleFullscreen->setMenuRole(QAction::NoRole);
    toggleFullscreen->setShortcut({"Meta+Ctrl+F"});
    // event filter will send new value to m_sFullscreen, so do post
    m_unsubscribe += toggleFullscreen->sTriggered().listen(post<unit>(this, [this](unit) {
        if (window()->isFullScreen())
            window()->setWindowState(window()->windowState() & ~Qt::WindowFullScreen);
        else
            window()->setWindowState(window()->windowState() | Qt::WindowFullScreen);
    }));

    viewMenu->addAction(toggleFullscreen);

    // video actions
    auto videoMenu = menubar->addMenu(tr("Video"));

    auto playStop = new SQAction(tr("Play/Pause"), videoItemSelected, videoMenu);
    playStop->setShortcut({"Space"});
    sTogglePlayVideo.loop(playStop->sTriggered());

    auto stepForward = new SQAction(tr("Step Forward"), videoItemSelected, videoMenu);
    stepForward->setShortcut({"."});
    const stream<qint64> sForward = stepForward->sTriggered().map(
        [](unit) { return qint64(10000); });

    auto stepBackward = new SQAction(tr("Step Backward"), videoItemSelected, videoMenu);
    stepBackward->setShortcut({","});
    const stream<qint64> sBackward = stepBackward->sTriggered().map(
        [](unit) { return qint64(-10000); });

    sStepVideo.loop(sForward.or_else(sBackward));

    videoMenu->addAction(playStop);
    videoMenu->addAction(stepForward);
    videoMenu->addAction(stepBackward);

    window()->installEventFilter(this);
}

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
        m_model->setSortKey(sortKey);
    }
    const auto rootPathValue = settings->value(kRootPath);
    if (rootPathValue.isValid())
        m_fileTree->setRootPath(rootPathValue.toString());
    const auto currentPathValue = settings->value(kCurrentPath);
    if (currentPathValue.isValid())
        m_fileTree->setCurrentPath(currentPathValue.toString());
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
    settings->setValue(kSortKey, int(m_model->sortKey()));
    settings->setValue(kRootPath, m_fileTree->rootPath());
    settings->setValue(kCurrentPath, m_fileTree->currentPath());
    m_settings.save(settings);
}

bool BrowserWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_progressIndicator->parentWidget() && event->type() == QEvent::Resize) {
        adaptProgressIndicator();
    } else if (watched == window() && event->type() == QEvent::WindowStateChange) {
        m_sFullscreen.send(isFullScreen());
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
