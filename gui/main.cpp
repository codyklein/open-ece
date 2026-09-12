#include "main_window.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("OpenECE");
    QApplication::setApplicationVersion("0.1.0");
    openece::gui::MainWindow window;
    window.show();
    return app.exec();
}
