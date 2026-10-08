#include "branding.hpp"
#include "main_window.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    openece::gui::configure_application_branding();
    openece::gui::MainWindow window;
    window.show();
    return app.exec();
}
