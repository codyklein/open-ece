#pragma once
#include "draft_view.hpp"
#include <openece/digital/circuit.hpp>

class QTableWidget;
class QComboBox;
class QSpinBox;
class QLabel;
class QFormLayout;

namespace openece::gui {
namespace digital_limits {
inline constexpr int inputs = 8;
inline constexpr int gates = 64;
inline constexpr int outputs = 16;
inline constexpr int pins = 8;
} // namespace digital_limits

class DigitalLogicView final : public DraftView {
  public:
    explicit DigitalLogicView(QWidget* parent = nullptr,
                              project::CombinationalDraft* draft = nullptr, bool inert = false);
    const project::CombinationalDraft& draft() const { return draft_; }
    digital::Circuit validated_circuit() const;
    void synchronize_pending_text() override;
    std::vector<digital::LogicValue> input_values() const;

  private:
    void refresh();
    void refresh_gate_editor();
    void refresh_sources();
    void invalidate();
    void evaluate(bool table);
    void update_gate_row(int row);
    QComboBox* source_selector(project::Reference source, const QString& name, QWidget* parent);
    void populate_sources(QComboBox* selector, project::Reference source);
    project::Id new_id();

    // The editable draft may be invalid. Only local validated Circuit snapshots are evaluated.
    // GUI-generated IDs start at 1; 0 is the GUI's explicit unconnected reference.
    DraftOwner<project::CombinationalDraft> state_;
    project::CombinationalDraft& draft_;
    bool refreshing_ = false;
    // Observing pointers; Qt parents own all widgets.
    QTableWidget* inputs_;
    QTableWidget* gates_;
    QTableWidget* outputs_;
    QTableWidget* truth_;
    QComboBox* kind_;
    QSpinBox* pin_count_;
    QFormLayout* pin_form_;
    QLabel* status_;
};
} // namespace openece::gui
