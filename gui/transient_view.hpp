#pragma once
#include "draft_view.hpp"
class QLabel;
class QTableWidget;
class QComboBox;
namespace openece::gui {
// Configuration only: this view has no numerical/runtime dependency or execution object.
class TransientView final : public DraftView {
    Q_OBJECT
  public:
    explicit TransientView(QWidget* parent = nullptr, project::TransientDraft* draft = nullptr);
    void synchronize_pending_text() override;

  private:
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
