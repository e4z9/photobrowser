#pragma once

#include "mediadirectorymodel.h"

#include <sqaction.h>
#include <sqtools.h>

#include <QMainWindow>
#include <QSettings>
#include <QTimer>

#include <sodium/sodium.h>

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

    sodium::stream_sink<bool> m_sFullscreen;
    FullscreenSplitter *m_splitter = nullptr;
    DirectoryTree *m_fileTree = nullptr;
    sodium::stream_sink<bool> m_sIsRecursiveFromSettings;
    sodium::cell<bool> m_isRecursive;
    QAction *m_sortExif = nullptr;
    QAction *m_sortFileName = nullptr;
    QAction *m_sortRandom = nullptr;
    Utils::ProgressIndicator *m_progressIndicator = nullptr;
    QTimer m_progressTimer;
    std::unique_ptr<MediaDirectoryModel> m_model;
    Unsubscribe m_unsubscribe;
};
