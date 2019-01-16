#include "browserwindow.h"

#include "directorytree.h"
#include "filmrollview.h"

#include <qtc/progressindicator.h>

#include <QAction>
#include <QCheckBox>
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

    auto moveToTrash = fileMenu->addAction(tr("Move to Trash"));
    moveToTrash->setShortcuts({{"Delete"}, {"Backspace"}});
    connect(moveToTrash, &QAction::triggered, this, [this, imageView] {
        m_model.moveItemAtIndexToTrash(imageView->currentIndex());
    });

    connect(imageView, &FilmRollView::currentItemChanged, this, [imageView, moveToTrash] {
        moveToTrash->setEnabled(imageView->currentItem().has_value());
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

    connect(imageView, &FilmRollView::currentItemChanged, this, [imageView, playStop, stepForward, stepBackward] {
        const auto currentItem = imageView->currentItem();
        const bool enabled = (currentItem && currentItem->type == MediaType::Video);
        playStop->setEnabled(enabled);
        stepForward->setEnabled(enabled);
        stepBackward->setEnabled(enabled);
    });
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
