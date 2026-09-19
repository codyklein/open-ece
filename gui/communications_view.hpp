#pragma once
#include <QWidget>
#include <memory>
#include <openece/communications/ber.hpp>
#include <openece/communications/link.hpp>
#include <optional>
class QComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QTableWidget;
class QLabel;
class QTimer;
class QTabWidget;
class QPushButton;
namespace openece::gui {
namespace communications_gui_limits {
inline constexpr int bits = 65536, samples = 65536, points = 41, bits_per_point = 1000000;
inline constexpr std::uint64_t aggregate_bits = 10000000, chunk_bits = 4096;
} // namespace communications_gui_limits
class CommunicationsPlot;
class CommunicationsView final : public QWidget {
  public:
    explicit CommunicationsView(QWidget* parent = nullptr);

  private:
    void invalidate();
    void simulate();
    void ensure_experiment();
    void advance();
    void render_ber();
    void cancel();
    communications::Modulation modulation() const;
    QComboBox *modulation_, *source_, *rate_unit_;
    QLineEdit *manual_, *rate_, *eb_, *bit_seed_, *noise_seed_, *start_, *stop_;
    QSpinBox *count_, *samples_, *points_, *budget_;
    QCheckBox* noise_;
    QTableWidget *bits_, *ber_table_;
    QLabel *status_, *link_summary_;
    QTabWidget* tabs_;
    QTimer* timer_;
    QPushButton* cancel_;
    CommunicationsPlot *i_plot_, *q_plot_, *constellation_, *ber_plot_;
    std::optional<communications::LinkResult> link_;
    std::unique_ptr<communications::BerExperiment> experiment_;
    communications::Modulation experiment_modulation_ = communications::Modulation::bpsk;
    bool loading_ = true, cancelled_ = false;
    int previous_unit_ = 1;
};
} // namespace openece::gui
