#include "app.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    App app;
    return QApplication::exec();
}
