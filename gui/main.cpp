#include "main_window.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("OpenECE");
    QApplication::setApplicationName("OpenECE");
    QApplication::setApplicationVersion(OPENECE_VERSION);
    openece::gui::MainWindow window;
    window.show();
    return app.exec();
}
