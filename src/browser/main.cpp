#include "app.h"

#include <QApplication>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    App app;
    QTranslator translator;
    if (translator.load(QLocale(), "photobrowser", "_", ":/i18n"))
        QCoreApplication::installTranslator(&translator);
    return QApplication::exec();
}
