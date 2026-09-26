#pragma once
#include "project_workflow.hpp"
#include <QMainWindow>
class QMenu;
namespace openece::gui {
class MainWindow final : public QMainWindow {
  public:
    explicit MainWindow(QWidget* parent = nullptr);
    MainWindow(std::unique_ptr<ProjectDocument>, std::shared_ptr<ProjectDialogs>,
               std::shared_ptr<ProjectPreferences>, QWidget* parent = nullptr);
    ~MainWindow() override;
    ProjectDocument& document() const { return *document_; }
    ProjectWorkflow& workflow() const { return *workflow_; }

  protected:
    void closeEvent(QCloseEvent*) override;

  private:
    void update_presentation();
    void update_recent_menu();
    std::unique_ptr<ProjectDocument> document_;
    std::unique_ptr<ProjectWorkflow> workflow_;
    QMenu* recent_ = nullptr;
};
} // namespace openece::gui
