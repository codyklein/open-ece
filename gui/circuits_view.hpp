#pragma once
#include <QWidget>
#include <cstdint>
class QComboBox;
class QLabel;
class QTableWidget;
namespace openece::gui {
namespace circuit_gui_limits {
inline constexpr int nodes = 32, components = 128, voltage_sources = 32;
}
class CircuitsView final : public QWidget {
  public:
    explicit CircuitsView(QWidget* parent = nullptr);

  private:
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
    std::uint64_t next_node_ = 0, next_component_ = 0;
    bool loading_ = false;
};
} // namespace openece::gui
