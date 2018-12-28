#include "browserwindow.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    BrowserWindow w;
    w.setGeometry(100, 100, 800, 550);
    w.show();
    QTimer::singleShot(0, &w, [&w]() { w.setFocus(); });
    return a.exec();
}
