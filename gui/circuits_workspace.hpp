#pragma once
#include "draft_view.hpp"
namespace openece::gui {
class CircuitsWorkspace final : public DraftView {
  public:
    explicit CircuitsWorkspace(QWidget* parent = nullptr, project::CircuitsDraft* draft = nullptr,
                               bool inert = false);
    void synchronize_pending_text() override;

  private:
    DraftOwner<project::CircuitsDraft> state_;
};
} // namespace openece::gui
