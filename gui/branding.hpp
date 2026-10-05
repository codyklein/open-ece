#pragma once
#include <QIcon>
namespace openece::gui {
// Explicit initialization keeps resources reachable through the static workbench library.
QIcon application_icon();
void configure_application_branding();
} // namespace openece::gui
