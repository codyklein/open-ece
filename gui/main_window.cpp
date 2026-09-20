#include "main_window.hpp"
#include "project_workspace.hpp"
namespace openece::gui {
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("OpenECE — Signals / DSP, Digital Logic, Circuits and Communications");
    resize(1280, 820);
    setCentralWidget(new ProjectWorkspace(project::default_project(), false, this));
}
} // namespace openece::gui
