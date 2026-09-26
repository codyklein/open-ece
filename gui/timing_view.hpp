#pragma once
#include "draft_view.hpp"
#include <memory>
#include <openece/digital/simulation.hpp>

class QTableWidget;
class QLabel;
class QLineEdit;
class QComboBox;
class QTimer;
namespace openece::gui {
namespace timing_gui_limits {
inline constexpr int inputs = 8, elements = 64, outputs = 16, pins = 8, observed = 16;
inline constexpr int stimuli = 10'000;
inline constexpr std::size_t queued = 10'000, processed = 100'000, pin_visits = 5'000'000,
                             recorded = 50'000;
inline constexpr int batch_timestamps = 100;
} // namespace timing_gui_limits
class TimingDiagramWidget;
class TimingView final : public DraftView {
  public:
    explicit TimingView(QWidget* parent = nullptr, project::TimingDraft* draft = nullptr,
                        bool inert = false);
    const project::TimingDraft& draft() const { return state_.get(); }
    void load_combinational(const digital::Circuit& circuit,
                            const std::vector<digital::LogicValue>& inputs,
                            digital::timing::Delay delay);

  private:
    DraftOwner<project::TimingDraft> state_;
    void restore_rows();
    void text_edit(QTableWidget*, int, int, const QString&);
    void invalidate();
    void fail(const std::exception& error);
    void initialize();
    void advance(bool run);
    void render();
    void append(QTableWidget* table, const QStringList& values, bool node = false);
    void example();
    std::uint32_t new_id();
    QTableWidget *inputs_, *elements_, *outputs_, *stimuli_, *results_;
    QLabel* status_;
    QLineEdit *horizon_, *observed_;
    QComboBox* scale_;
    QTimer* timer_;
    TimingDiagramWidget* diagram_;
    std::unique_ptr<digital::timing::Simulation> simulation_;
    std::vector<std::string> output_names_;
    QStringList trace_labels_;
};
} // namespace openece::gui
