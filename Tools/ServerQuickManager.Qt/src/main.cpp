#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("ServerQuickManagerQt");
    QApplication::setOrganizationName("RunixMu");

    MainWindow window;
    window.show();

    return app.exec();
}
