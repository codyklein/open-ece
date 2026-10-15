#pragma once
#include "draft_view.hpp"
#include "transient_runner.hpp"
class QLabel;
class QTableWidget;
class QComboBox;
class QPushButton;
class QTimer;
class QProgressBar;
namespace openece::gui {
class TransientTraceModel;
class TransientPlot;
class TransientView final : public DraftView {
    Q_OBJECT
  public:
    explicit TransientView(QWidget* parent = nullptr, project::TransientDraft* draft = nullptr);
    ~TransientView() override;
    void synchronize_pending_text() override;
    void stop_execution();
    std::shared_ptr<const transient_core::Result> result() const { return result_; }
    const std::vector<TransientProbeLabel>& result_probes() const { return labels_; }

  private:
    enum class ExecutionState { ready, running, paused, complete, cancelled, failed, stale };
    ExecutionState execution_state_ = ExecutionState::ready;
    std::unique_ptr<TransientRunner> runner_;
    std::shared_ptr<const transient_core::Result> result_;
    std::vector<TransientProbeLabel> labels_;
    project::TransientDraft run_draft_;
    QString time_unit_ = "s";
    double time_scale_ = 1;
    double stop_seconds_ = 1;
    QProgressBar* progress_;
    bool pending_ = false;
    QTimer* poll_;
    QLabel* diagnostic_;
    QPushButton *run_, *pause_, *step_, *cancel_, *reset_;
    QComboBox *voltage_select_, *current_select_;
    TransientPlot *voltage_plot_, *current_plot_;
    TransientTraceModel* trace_model_;
    void start_execution(bool single);
    void receive_update();
    void update_execution_ui();
    void update_plots();
    void reset_results();
    void invalidate_execution();

    DraftOwner<project::TransientDraft> state_;
    QTableWidget *nodes_, *components_, *points_, *initial_, *probes_;
    QComboBox *ground_, *source_component_, *source_mode_;
    QLabel* status_;
    bool rendering_ = false;
    int source_row_ = -1;
    void render();
    void render_points();
    void refresh_references();
    void text_edit(QTableWidget*, int, int, const QString&);
    void change_component_unit(int, QComboBox*);
    void report(const QString&);
};
} // namespace openece::gui
