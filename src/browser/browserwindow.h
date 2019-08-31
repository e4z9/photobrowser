#pragma once

#include "mediadirectorymodel.h"

#include "tools.h"

#include <QMainWindow>
#include <QSettings>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QCheckBox;
QT_END_NAMESPACE

class DirectoryTree;
class FullscreenSplitter;

namespace Utils { class ProgressIndicator; }

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    BrowserWindow(QWidget *parent = nullptr);

    void restore(QSettings *settings);
    void save(QSettings *settings);

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void adaptProgressIndicator();
    void updateWindowTitle(const std::optional<MediaItem> &item);

    FullscreenSplitter *m_splitter = nullptr;
    DirectoryTree *m_fileTree = nullptr;
    QCheckBox *m_recursive = nullptr;
    QAction *m_sortExif = nullptr;
    QAction *m_sortFileName = nullptr;
    QAction *m_sortRandom = nullptr;
    QAction *m_toggleFullscreen = nullptr;
    Utils::ProgressIndicator *m_progressIndicator = nullptr;
    QTimer m_progressTimer;
    MediaDirectoryModel m_model;
    Unsubscribe m_unsubscribe;
};
