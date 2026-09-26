#pragma once
#include "draft_view.hpp"
namespace openece::gui {
class DigitalWorkspace final : public DraftView {
  public:
    explicit DigitalWorkspace(QWidget* parent = nullptr, project::DigitalDraft* draft = nullptr,
                              bool inert = false);
    void synchronize_pending_text() override;

  private:
    DraftOwner<project::DigitalDraft> state_;
};
} // namespace openece::gui
