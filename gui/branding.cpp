#include "branding.hpp"
#include <QApplication>
#include <QResource>

// Q_INIT_RESOURCE declares a global resource symbol, so keep this outside namespaces.
static void initialize_brand_resources() {
    static const bool initialized = [] {
        Q_INIT_RESOURCE(openece);
        return true;
    }();
    (void)initialized;
}
namespace openece::gui {
QIcon application_icon() {
    initialize_brand_resources();
    QIcon icon;
    for (const int size : {16, 20, 24, 32, 40, 48, 64, 96, 128, 256, 512, 1024})
        icon.addFile(QString(":/branding/openece-%1.png").arg(size), QSize(size, size));
    return icon;
}
void configure_application_branding() {
    QApplication::setOrganizationName("OpenECE");
    QApplication::setApplicationName("OpenECE");
    QApplication::setApplicationVersion(OPENECE_VERSION);
    QApplication::setWindowIcon(application_icon());
}
} // namespace openece::gui
