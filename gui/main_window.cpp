#include "main_window.hpp"
#include <QAction>
#include <QFileInfo>
#include <QMenuBar>
#include <QStatusBar>
namespace openece::gui {
MainWindow::MainWindow(QWidget* parent)
    : MainWindow(std::make_unique<ProjectDocument>(
                     std::make_unique<ProjectWorkspace>(project::default_project(), false)),
                 {}, std::make_shared<SettingsProjectPreferences>(), parent) {}
MainWindow::MainWindow(std::unique_ptr<ProjectDocument> document,
                       std::shared_ptr<ProjectDialogs> dialogs,
                       std::shared_ptr<ProjectPreferences> preferences, QWidget* parent)
    : QMainWindow(parent), document_(std::move(document)) {
    resize(1280, 820);
    if (!dialogs)
        dialogs = qt_project_dialogs(this);
    workflow_ =
        std::make_unique<ProjectWorkflow>(*document_, std::move(dialogs), std::move(preferences));
    setCentralWidget(&document_->workspace());
    connect(document_.get(), &ProjectDocument::workspaceReplaced, this,
            [this](ProjectWorkspace*, ProjectWorkspace* next) {
                // The document, not QMainWindow, deletes the old workspace after this signal.
                takeCentralWidget();
                setCentralWidget(next);
            });
    connect(document_.get(), &ProjectDocument::stateChanged, this,
            &MainWindow::update_presentation);
    auto* file = menuBar()->addMenu("&File");
    auto action = [this, file](const QString& name, const QString& text,
                               QKeySequence::StandardKey key, auto callback) {
        auto* a = file->addAction(text);
        a->setObjectName(name);
        a->setShortcuts(key);
        connect(a, &QAction::triggered, this, callback);
    };
    action("project_new", "&New", QKeySequence::New, [this] { workflow_->new_project(); });
    action("project_open", "&Open…", QKeySequence::Open, [this] { workflow_->open_project(); });
    action("project_save", "&Save", QKeySequence::Save, [this] { workflow_->save(); });
    action("project_save_as", "Save &As…", QKeySequence::SaveAs, [this] { workflow_->save_as(); });
    recent_ = file->addMenu("Recent &Projects");
    recent_->setObjectName("project_recent");
    connect(recent_, &QMenu::aboutToShow, this, &MainWindow::update_recent_menu);
    file->addSeparator();
    action("project_close", "&Close", QKeySequence::Close, [this] { close(); });
    action("project_exit", "E&xit", QKeySequence::Quit, [this] { close(); });
    update_presentation();
}
MainWindow::~MainWindow() {
    takeCentralWidget();
    workflow_.reset();
    document_.reset();
}
void MainWindow::closeEvent(QCloseEvent* event) {
    if (workflow_->request_close())
        event->accept();
    else
        event->ignore();
}
void MainWindow::update_presentation() {
    setWindowTitle(project_window_title(*document_));
    setWindowFilePath(document_->path());
    setToolTip(document_->path());
    statusBar()->showMessage(document_->path().isEmpty() ? "Untitled project" : document_->path());
}
void MainWindow::update_recent_menu() {
    recent_->clear();
    const auto& paths = workflow_->recent_projects();
    if (paths.isEmpty()) {
        recent_->addAction("No recent projects")->setEnabled(false);
        return;
    }
    for (const auto& path : paths) {
        const QString label = path + (QFileInfo::exists(path) ? "" : " (missing)");
        auto* a = recent_->addAction(QString(label).replace("&", "&&"));
        a->setToolTip(path);
        connect(a, &QAction::triggered, this, [this, path] { workflow_->open_path(path); });
    }
    recent_->addSeparator();
    auto* remove = recent_->addMenu("Remove from recent projects");
    for (const auto& path : paths) {
        auto* a = remove->addAction(QString(path).replace("&", "&&"));
        connect(a, &QAction::triggered, this, [this, path] { workflow_->remove_recent(path); });
    }
}
} // namespace openece::gui
