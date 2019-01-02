#include "browserwindow.h"

#include "directorytree.h"
#include "filmrollview.h"

#include <qtc/progressindicator.h>

#include <QCheckBox>
#include <QEvent>
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
