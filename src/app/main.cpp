#include "ui/MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    reader::ui::MainWindow window;
    window.show();

    return app.exec();
}
