#include "circuits_workspace.hpp"
#include "ac_view.hpp"
#include "circuits_view.hpp"
#include <QTabWidget>
#include <QVBoxLayout>
namespace openece::gui {
CircuitsWorkspace::CircuitsWorkspace(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName("circuits_analysis_tabs");
    tabs->addTab(new CircuitsView(tabs), "DC");
    tabs->addTab(new AcView(tabs), "AC / Phasors");
    layout->addWidget(tabs);
}
} // namespace openece::gui
