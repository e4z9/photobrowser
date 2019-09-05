#include "app.h"

#include <QCoreApplication>
#include <QFileOpenEvent>
#include <QTimer>

#include <gst/gst.h>

const char kMainWindow[] = "MainWindow";

static void withGroup(QSettings *settings,
                      const QString &group,
                      std::function<void(QSettings *)> fun)
{
    settings->beginGroup(group);
    fun(settings);
    settings->endGroup();
}

App::App(QObject *parent)
    : QObject(parent)
{
    QCoreApplication::instance()->installEventFilter(this);
    QCoreApplication::setApplicationName("Photobrowser");
    QCoreApplication::setOrganizationName("Zillerey");
    QCoreApplication::setOrganizationDomain("zillerey.de");
    QTimer::singleShot(0, this, [this] {
        if (!m_settings)
            createWindow({});
    });

    gst_init(nullptr, nullptr);
}

App::~App()
{
    gst_deinit();
}

bool App::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == QCoreApplication::instance()) {
        if (ev->type() == QEvent::FileOpen) {
            QFileOpenEvent *foe = static_cast<QFileOpenEvent *>(ev);
            createWindow(foe->file() + "/settings.ini");
        }
    }
    return QObject::eventFilter(obj, ev);
}

void App::createWindow(const QString &settingsFilePath)
{
    if (settingsFilePath.isEmpty())
        m_settings = std::make_unique<QSettings>();
    else
        m_settings = std::make_unique<QSettings>(settingsFilePath, QSettings::IniFormat);
    m_window = std::make_unique<BrowserWindow>();
    m_window->setGeometry(100, 100, 800, 550);
    withGroup(m_settings.get(), kMainWindow, [this](QSettings *s) { m_window->restore(s); });
    QTimer::singleShot(0, m_window.get(), [this]() { m_window->setFocus(); });
    QObject::connect(QCoreApplication::instance(),
                     &QCoreApplication::aboutToQuit,
                     m_window.get(),
                     [this] {
                         withGroup(m_settings.get(), kMainWindow, [this](QSettings *s) {
                             m_window->save(s);
                         });
                     });
    m_window->show();
}
