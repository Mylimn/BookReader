#include "ui/MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 窗口与任务栏图标（编译进 qrc；exe 文件图标另见 resources/Reader.rc）
    app.setWindowIcon(QIcon(QStringLiteral(":/reader.ico")));

    reader::ui::MainWindow window;
    window.show();

    return app.exec();
}
