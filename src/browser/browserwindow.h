#pragma once

#include "mediadirectorymodel.h"

#include <QMainWindow>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QCheckBox;
QT_END_NAMESPACE

class DirectoryTree;

namespace Utils { class ProgressIndicator; }

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    BrowserWindow(QWidget *parent = nullptr);

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void adaptProgressIndicator();

    DirectoryTree *m_fileTree = nullptr;
    QCheckBox *m_recursive = nullptr;
    Utils::ProgressIndicator *m_progressIndicator = nullptr;
    QTimer m_progressTimer;
    MediaDirectoryModel m_model;
};
