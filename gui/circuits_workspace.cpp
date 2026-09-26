#include "circuits_workspace.hpp"
#include "ac_view.hpp"
#include "circuits_view.hpp"
#include <QTabWidget>
#include <QVBoxLayout>
namespace openece::gui {
CircuitsWorkspace::CircuitsWorkspace(QWidget* parent, project::CircuitsDraft* draft, bool inert)
    : DraftView(parent), state_(draft, project::default_project().circuits) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName("circuits_analysis_tabs");
    tabs->addTab(new CircuitsView(tabs, &state_.get().dc, inert), "DC");
    tabs->addTab(new AcView(tabs, &state_.get().ac, inert), "AC / Phasors");
    layout->addWidget(tabs);
    bind_tabs(tabs, state_.get().selected_tab, {"dc", "ac"});
    for (auto* view : findChildren<DraftView*>())
        connect(view, &DraftView::draftEdited, this, &DraftView::draftEdited);
    restoring_ = false;
}
void CircuitsWorkspace::synchronize_pending_text() {
    DraftView::synchronize_pending_text();
    for (auto* view : findChildren<DraftView*>())
        view->synchronize_pending_text();
}
} // namespace openece::gui
