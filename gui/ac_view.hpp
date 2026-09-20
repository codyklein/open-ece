#pragma once
#include "circuit_draft_rows.hpp"
#include "draft_view.hpp"
#include <openece/circuits/ac/sweep.hpp>
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QTimer;
namespace openece::gui {
namespace ac_gui_limits {
inline constexpr int nodes = 32, components = 128, voltage_sources = 32, sweep_points = 1001;
}
class AcResponsePlot;
class AcView final : public DraftView {
  public:
    explicit AcView(QWidget* parent = nullptr, project::AcDraft* draft = nullptr,
                    bool inert = false);
    const project::AcDraft& draft() const { return state_.get(); }

  private:
    DraftOwner<project::AcDraft> state_;
    std::unique_ptr<CircuitDraftRows<project::AcDraft>> rows_;
    void invalidate();
    void add_node(const QString& name = {});
    void add_component();
    void refresh_connection(QComboBox*);
    void refresh_connections();
    void refresh_sources();
    void connect_units(QComboBox*, QLineEdit*);
    void load_example(bool rlc);
    circuits::ac::Circuit validated_circuit() const;
    void solve();
    void start_sweep();
    void advance_sweep();
    void cancel_sweep();
    void update_plots();
    QTableWidget *nodes_, *components_, *voltages_, *currents_, *sweep_;
    QComboBox *ground_, *probe_positive_, *probe_negative_, *reference_, *mode_, *spacing_;
    QLineEdit *frequency_, *start_, *stop_;
    QComboBox *frequency_unit_, *start_unit_, *stop_unit_;
    QSpinBox* count_;
    QTabWidget* tabs_;
    QLabel* status_;
    QPushButton* cancel_;
    QTimer* timer_;
    AcResponsePlot *magnitude_plot_, *phase_plot_;
    bool loading_ = false, normalized_ = false, logarithmic_ = true;
    std::optional<circuits::ac::Circuit> run_circuit_;
    circuits::ac::VoltageProbe probe_{};
    circuits::ComponentId source_{};
    std::vector<double> frequencies_;
    std::vector<circuits::ac::SweepPoint> points_;
    std::vector<std::optional<double>> magnitudes_, phases_;
    std::size_t failures_ = 0;
};
} // namespace openece::gui
