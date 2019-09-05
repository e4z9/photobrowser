#pragma once

#include "browserwindow.h"

#include <QObject>
#include <QSettings>

#include <memory>

class App : public QObject
{
    Q_OBJECT
public:
    explicit App(QObject *parent = nullptr);
    ~App() override;

    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    void createWindow(const QString &settingsFilePath);

    std::unique_ptr<QSettings> m_settings;
    std::unique_ptr<BrowserWindow> m_window;
};
