#pragma once

#include "mediadirectorymodel.h"

#include <QMainWindow>

class DirectoryTree;

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    BrowserWindow(QWidget *parent = 0);
    ~BrowserWindow();

private:
    DirectoryTree *m_fileTree;
    MediaDirectoryModel m_model;
};
