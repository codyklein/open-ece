#pragma once
#include "circuit_draft_rows.hpp"
#include "draft_view.hpp"
#include <cstdint>
class QComboBox;
class QLabel;
class QTableWidget;
class QTabWidget;
namespace openece::gui {
namespace circuit_gui_limits {
inline constexpr int nodes = 32, components = 128, voltage_sources = 32;
}
class CircuitsView final : public DraftView {
  public:
    explicit CircuitsView(QWidget* parent = nullptr, project::DcDraft* draft = nullptr,
                          bool inert = false);
    const project::DcDraft& draft() const { return state_.get(); }

  private:
    DraftOwner<project::DcDraft> state_;
    std::unique_ptr<CircuitDraftRows<project::DcDraft>> rows_;
    void invalidate();
    void add_node(const QString& name = {});
    void add_component();
    void refresh_connections();
    void refresh_connection(QComboBox* selector);
    void load_divider();
    void solve();
    QTableWidget *nodes_, *components_, *voltages_, *currents_;
    QComboBox* ground_;
    QLabel* status_;
    QTabWidget* tabs_;
    bool loading_ = false;
};
} // namespace openece::gui
