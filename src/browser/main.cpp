#include "browserwindow.h"

#include <QApplication>
#include <QTimer>

const char kMainWindow[] = "MainWindow";

void withGroup(QSettings *settings, const QString &group, std::function<void(QSettings *)> fun)
{
    settings->beginGroup(group);
    fun(settings);
    settings->endGroup();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName("Photobrowser");
    QApplication::setOrganizationName("Zillerey");
    QApplication::setOrganizationDomain("zillerey.de");
    QSettings settings;
    BrowserWindow w;
    w.setGeometry(100, 100, 800, 550);
    withGroup(&settings, kMainWindow, [&w](QSettings *s) { w.restore(s); });
    w.show();
    QTimer::singleShot(0, &w, [&w]() { w.setFocus(); });
    QObject::connect(&a, &QGuiApplication::aboutToQuit, &w, [&w, &settings] {
        withGroup(&settings, kMainWindow, [&w](QSettings *s) { w.save(s); });
    });
    return QApplication::exec();
}
