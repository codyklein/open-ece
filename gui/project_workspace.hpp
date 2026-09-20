#pragma once
#include "draft_view.hpp"
namespace openece::gui {
// One owned editable snapshot. Child views borrow subdrafts; the QObject tree and
// simulation/plot objects are destroyed before the snapshot. No file/controller state.
class ProjectWorkspace final : public DraftView {
  public:
    explicit ProjectWorkspace(project::ProjectSnapshot snapshot = project::default_project(),
                              bool inert = true, QWidget* parent = nullptr);
    ~ProjectWorkspace() override;
    void synchronize_pending_text() override;
    project::ProjectSnapshot capture();
    const project::ProjectSnapshot& draft() const { return snapshot_; }

  private:
    project::ProjectSnapshot snapshot_;
    QWidget* host_ = nullptr;
};
} // namespace openece::gui
