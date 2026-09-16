#include "digital_logic_view.hpp"
#include <openece/digital/truth_table.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <array>
#include <limits>
#include <stdexcept>

namespace openece::gui {
namespace {
using namespace digital;
constexpr std::array kinds{GateKind::Not, GateKind::And, GateKind::Or,  GateKind::Nand,
                           GateKind::Nor, GateKind::Xor, GateKind::Xnor};
const QStringList kind_names{"NOT", "AND", "OR", "NAND", "NOR", "XOR", "XNOR"};
QString kind_name(GateKind kind) {
    for (std::size_t i = 0; i < kinds.size(); ++i) {
        if (kinds[i] == kind)
            return kind_names[static_cast<int>(i)];
    }
    return "Invalid";
}
QString value_text(LogicValue value) { return value == LogicValue::one ? "1" : "0"; }
QTableWidgetItem* cell(const QString& text, bool editable = false) {
    auto* item = new QTableWidgetItem(text);
    if (!editable)
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}
QTableWidget* table(QWidget* parent, const char* name, const QStringList& headers) {
    auto* widget = new QTableWidget(0, static_cast<int>(headers.size()), parent);
    widget->setObjectName(name);
    widget->setHorizontalHeaderLabels(headers);
    widget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    widget->setSelectionBehavior(QAbstractItemView::SelectRows);
    widget->setSelectionMode(QAbstractItemView::SingleSelection);
    widget->setMinimumHeight(120);
    return widget;
}
QPushButton* button(QHBoxLayout* row, const QString& text, const char* name) {
    auto* result = new QPushButton(text);
    result->setObjectName(name);
    row->addWidget(result);
    return result;
}
} // namespace

DigitalLogicView::DigitalLogicView(QWidget* parent) : QWidget(parent) {
    setObjectName("digital_logic_view");
    draft_ = {{{{1}, "A"}, {{2}, "B"}},
              {{{3}, GateKind::Xor, {{1}, {2}}}, {{4}, GateKind::And, {{1}, {2}}}},
              {{"Sum", {3}}, {"Carry", {4}}}};
    assignments_ = {LogicValue::zero, LogicValue::zero};
    auto* layout = new QVBoxLayout(this);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* content = new QWidget(scroll);
    auto* columns = new QHBoxLayout(content);
    auto* left = new QVBoxLayout;
    auto* right = new QVBoxLayout;
    columns->addLayout(left, 3);
    columns->addLayout(right, 2);
    left->addWidget(new QLabel("Primary inputs — double-click a name to edit; check = 1", content));
    inputs_ = table(content, "digital_inputs", {"ID", "Name", "Value"});
    inputs_->setFixedHeight(120);
    left->addWidget(inputs_);
    auto* input_buttons = new QHBoxLayout;
    auto* add_input = button(input_buttons, "Add input", "digital_add_input");
    auto* remove_input = button(input_buttons, "Remove input", "digital_remove_input");
    left->addLayout(input_buttons);
    left->addWidget(new QLabel("Gates — select a row to edit its type and connections", content));
    gates_ = table(content, "digital_gates", {"ID", "Type", "Source IDs (pin order)", "Value"});
    gates_->setFixedHeight(120);
    left->addWidget(gates_);
    auto* gate_buttons = new QHBoxLayout;
    auto* add_gate = button(gate_buttons, "Add gate", "digital_add_gate");
    auto* remove_gate = button(gate_buttons, "Remove gate", "digital_remove_gate");
    left->addLayout(gate_buttons);
    left->addWidget(new QLabel("Primary outputs — names and source nodes", content));
    outputs_ = table(content, "digital_outputs", {"Name", "Source", "Value"});
    outputs_->setFixedHeight(120);
    left->addWidget(outputs_);
    auto* output_buttons = new QHBoxLayout;
    auto* add_output = button(output_buttons, "Add output", "digital_add_output");
    auto* remove_output = button(output_buttons, "Remove output", "digital_remove_output");
    left->addLayout(output_buttons);
    left->addStretch();
    auto* editor = new QGroupBox("Selected gate", content);
    auto* editor_layout = new QVBoxLayout(editor);
    auto* form = new QFormLayout;
    kind_ = new QComboBox(editor);
    kind_->setObjectName("digital_gate_kind");
    kind_->addItems(kind_names);
    pin_count_ = new QSpinBox(editor);
    pin_count_->setObjectName("digital_pin_count");
    pin_count_->setRange(1, digital_limits::pins);
    form->addRow("Type", kind_);
    form->addRow("Input pins", pin_count_);
    editor_layout->addLayout(form);
    pin_form_ = new QFormLayout;
    editor_layout->addLayout(pin_form_);
    auto* pin_help = new QLabel("NOT requires exactly one pin. New pins start unconnected. "
                                "Changing type preserves pins; adjust the count explicitly.",
                                editor);
    pin_help->setWordWrap(true);
    editor_layout->addWidget(pin_help);
    right->addWidget(editor);
    auto* help = new QTextBrowser(content);
    help->setObjectName("digital_help");
    help->setMinimumWidth(280);
    help->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    help->setHtml(
        "<h3>Combinational logic</h3><p>The initial circuit is a half-adder: Sum = A XOR B; "
        "Carry = A AND B. Edit it or add/remove nodes. Check the primary inputs, then Evaluate. "
        "Draft edits clear results; missing connections and cycles must be fixed explicitly.</p>"
        "<p>Two states: 0 and 1. NOT takes one pin; other gates take one or more. "
        "AND requires all 1; OR requires any 1. NAND/NOR invert those results. "
        "XOR is odd parity; XNOR is even parity, not an all-equal test. Unary "
        "AND/OR/XOR copy the input; unary NAND/NOR/XNOR invert it.</p>"
        "<p>Node IDs identify connections; names are exact, case-sensitive UTF-8 strings, "
        "nonempty and unique separately among inputs and outputs (128 bytes maximum). "
        "Whitespace is preserved. Deleting a source leaves its references visibly missing.</p>"
        "<p>Truth-table columns follow declaration order. Rows count upward in binary, "
        "first input most significant; unused inputs still appear. Tables do not change "
        "toggles.</p>"
        "<p>No propagation delay or time axis: evaluation finds settled Boolean values. "
        "All cycles are rejected, including disconnected ones. Diagnostics list unresolved/blocked "
        "gates, which can include downstream gates outside the cycle.</p>"
        "<p>Desktop limits: 8 inputs, 64 gates, 16 outputs, 8 pins per gate. "
        "Core limits: 64 inputs, 4096 gates, 256 outputs, 16384 references; tables at most "
        "10 inputs / 1024 rows. No saving, constants, buses, Unknown/High-Z, clocks, "
        "sequential logic, timing simulation or minimization in v0.4.</p>");
    right->addWidget(help, 1);
    for (auto* label : content->findChildren<QLabel*>())
        label->setWordWrap(true);
    scroll->setWidget(content);
    layout->addWidget(scroll, 4);
    auto* actions = new QHBoxLayout;
    auto* evaluate_button = button(actions, "Evaluate", "digital_evaluate");
    auto* truth_button = button(actions, "Generate truth table", "digital_truth");
    actions->addStretch();
    layout->addLayout(actions);
    status_ = new QLabel(this);
    status_->setObjectName("digital_status");
    status_->setWordWrap(true);
    status_->setTextFormat(Qt::PlainText);
    layout->addWidget(status_);
    truth_ = table(this, "digital_truth_table", {});
    truth_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(truth_, 1);

    connect(evaluate_button, &QPushButton::clicked, this, [this] { evaluate(false); });
    connect(truth_button, &QPushButton::clicked, this, [this] { evaluate(true); });
    connect(inputs_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (refreshing_ || item->column() != 1)
            return;
        draft_.inputs[static_cast<std::size_t>(item->row())].name =
            item->text().toUtf8().toStdString();
        invalidate();
        refresh_sources();
    });
    connect(outputs_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (refreshing_ || item->column() != 0)
            return;
        draft_.outputs[static_cast<std::size_t>(item->row())].name =
            item->text().toUtf8().toStdString();
        invalidate();
    });
    connect(gates_, &QTableWidget::currentCellChanged, this, [this] {
        if (!refreshing_)
            refresh_gate_editor();
    });
    connect(kind_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const int row = gates_->currentRow();
        if (refreshing_ || row < 0 || index < 0)
            return;
        draft_.gates[static_cast<std::size_t>(row)].kind = kinds[static_cast<std::size_t>(index)];
        update_gate_row(row);
        invalidate();
        refresh_sources();
    });
    connect(pin_count_, &QSpinBox::valueChanged, this, [this](int count) {
        const int row = gates_->currentRow();
        if (refreshing_ || row < 0)
            return;
        draft_.gates[static_cast<std::size_t>(row)].inputs.resize(static_cast<std::size_t>(count),
                                                                  NodeId{0});
        update_gate_row(row);
        invalidate();
        refresh_gate_editor();
    });
    connect(add_input, &QPushButton::clicked, this, [this] {
        if (draft_.inputs.size() >= digital_limits::inputs) {
            status_->setText(QString("Input limit reached (%1).").arg(digital_limits::inputs));
            return;
        }
        const auto id = new_id();
        if (id.value == 0)
            return;
        draft_.inputs.push_back({id, "Input " + std::to_string(id.value)});
        assignments_.push_back(LogicValue::zero);
        refresh();
    });
    connect(remove_input, &QPushButton::clicked, this, [this] {
        const int row = inputs_->currentRow();
        if (row < 0)
            return;
        draft_.inputs.erase(draft_.inputs.begin() + row);
        assignments_.erase(assignments_.begin() + row);
        refresh();
    });
    connect(add_gate, &QPushButton::clicked, this, [this] {
        if (draft_.gates.size() >= digital_limits::gates) {
            status_->setText(QString("Gate limit reached (%1).").arg(digital_limits::gates));
            return;
        }
        const auto id = new_id();
        if (id.value == 0)
            return;
        draft_.gates.push_back({id, GateKind::And, {{0}, {0}}});
        refresh();
        gates_->selectRow(static_cast<int>(draft_.gates.size()) - 1);
    });
    connect(remove_gate, &QPushButton::clicked, this, [this] {
        const int row = gates_->currentRow();
        if (row < 0)
            return;
        draft_.gates.erase(draft_.gates.begin() + row);
        refresh();
    });
    connect(add_output, &QPushButton::clicked, this, [this] {
        if (draft_.outputs.size() >= digital_limits::outputs) {
            status_->setText(QString("Output limit reached (%1).").arg(digital_limits::outputs));
            return;
        }
        // Use the monotonic ID counter only to suggest a fresh display name.
        const auto id = new_id();
        if (id.value == 0)
            return;
        draft_.outputs.push_back({"Output " + std::to_string(id.value), {0}});
        refresh();
    });
    connect(remove_output, &QPushButton::clicked, this, [this] {
        const int row = outputs_->currentRow();
        if (row < 0)
            return;
        draft_.outputs.erase(draft_.outputs.begin() + row);
        refresh();
    });
    refresh();
    evaluate(false);
}

NodeId DigitalLogicView::new_id() {
    if (next_id_ == std::numeric_limits<std::uint32_t>::max()) {
        status_->setText("Node ID space exhausted; reopen the application for a new draft.");
        return {0};
    }
    return {next_id_++};
}
void DigitalLogicView::populate_sources(QComboBox* selector, NodeId source) {
    const QSignalBlocker blocker(selector);
    selector->clear();
    selector->addItem("— unconnected —", 0U);
    for (const auto& input : draft_.inputs) {
        selector->addItem(
            QString("%1: %2").arg(input.id.value).arg(QString::fromStdString(input.name)),
            input.id.value);
    }
    for (const auto& gate : draft_.gates) {
        selector->addItem(QString("%1: %2").arg(gate.id.value).arg(kind_name(gate.kind)),
                          gate.id.value);
    }
    int index = selector->findData(source.value);
    if (index < 0) {
        selector->addItem(QString("Missing node %1").arg(source.value), source.value);
        index = selector->count() - 1;
    }
    selector->setCurrentIndex(index);
}
QComboBox* DigitalLogicView::source_selector(NodeId source, const QString& name, QWidget* parent) {
    auto* selector = new QComboBox(parent);
    selector->setObjectName(name);
    selector->setMinimumContentsLength(12);
    selector->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    populate_sources(selector, source);
    return selector;
}
void DigitalLogicView::refresh() {
    const int selected = gates_->currentRow();
    refreshing_ = true;
    inputs_->setRowCount(0);
    inputs_->setRowCount(static_cast<int>(draft_.inputs.size()));
    for (int i = 0; i < inputs_->rowCount(); ++i) {
        const auto index = static_cast<std::size_t>(i);
        inputs_->setItem(i, 0, cell(QString::number(draft_.inputs[index].id.value)));
        inputs_->setItem(i, 1, cell(QString::fromStdString(draft_.inputs[index].name), true));
        auto* toggle = new QCheckBox(value_text(assignments_[index]), inputs_);
        toggle->setObjectName(QString("digital_input_%1").arg(draft_.inputs[index].id.value));
        toggle->setChecked(assignments_[index] == LogicValue::one);
        inputs_->setCellWidget(i, 2, toggle);
        connect(toggle, &QCheckBox::toggled, this, [this, index, toggle](bool high) {
            assignments_[index] = high ? LogicValue::one : LogicValue::zero;
            toggle->setText(high ? "1" : "0");
            invalidate();
        });
    }
    gates_->setRowCount(0);
    gates_->setRowCount(static_cast<int>(draft_.gates.size()));
    for (int i = 0; i < gates_->rowCount(); ++i)
        update_gate_row(i);
    outputs_->setRowCount(0);
    outputs_->setRowCount(static_cast<int>(draft_.outputs.size()));
    for (int i = 0; i < outputs_->rowCount(); ++i) {
        const auto index = static_cast<std::size_t>(i);
        outputs_->setItem(i, 0, cell(QString::fromStdString(draft_.outputs[index].name), true));
        auto* selector = source_selector(draft_.outputs[index].source,
                                         QString("digital_output_source_%1").arg(i), outputs_);
        outputs_->setCellWidget(i, 1, selector);
        outputs_->setItem(i, 2, cell("—"));
        connect(selector, &QComboBox::currentIndexChanged, this, [this, index, selector] {
            draft_.outputs[index].source = {selector->currentData().toUInt()};
            invalidate();
        });
    }
    if (gates_->rowCount() > 0)
        gates_->selectRow(qBound(0, selected, gates_->rowCount() - 1));
    refreshing_ = false;
    refresh_gate_editor();
    invalidate();
}
void DigitalLogicView::update_gate_row(int row) {
    const auto& gate = draft_.gates[static_cast<std::size_t>(row)];
    gates_->setItem(row, 0, cell(QString::number(gate.id.value)));
    gates_->setItem(row, 1, cell(kind_name(gate.kind)));
    QStringList sources;
    for (auto source : gate.inputs) {
        bool found = false;
        for (const auto& input : draft_.inputs)
            found = found || input.id == source;
        for (const auto& candidate : draft_.gates)
            found = found || candidate.id == source;
        sources.push_back(source.value == 0 ? "unconnected"
                                            : (found ? QString::number(source.value)
                                                     : QString("missing %1").arg(source.value)));
    }
    gates_->setItem(row, 2, cell(sources.join(", ")));
    gates_->setItem(row, 3, cell("—"));
}
void DigitalLogicView::refresh_gate_editor() {
    const QSignalBlocker block_kind(kind_), block_count(pin_count_);
    while (pin_form_->rowCount() > 0)
        pin_form_->removeRow(0);
    const int row = gates_->currentRow();
    kind_->setEnabled(row >= 0);
    pin_count_->setEnabled(row >= 0);
    if (row < 0)
        return;
    const auto index = static_cast<std::size_t>(row);
    const auto& gate = draft_.gates[index];
    kind_->setCurrentText(kind_name(gate.kind));
    pin_count_->setValue(static_cast<int>(gate.inputs.size()));
    for (std::size_t pin = 0; pin < gate.inputs.size(); ++pin) {
        auto* selector = source_selector(gate.inputs[pin], QString("digital_pin_%1").arg(pin),
                                         kind_->parentWidget());
        pin_form_->addRow(QString("Pin %1").arg(pin + 1), selector);
        connect(selector, &QComboBox::currentIndexChanged, this, [this, index, pin, selector] {
            draft_.gates[index].inputs[pin] = {selector->currentData().toUInt()};
            update_gate_row(static_cast<int>(index));
            invalidate();
        });
    }
}
void DigitalLogicView::refresh_sources() {
    refresh_gate_editor();
    for (int i = 0; i < outputs_->rowCount(); ++i) {
        populate_sources(static_cast<QComboBox*>(outputs_->cellWidget(i, 1)),
                         draft_.outputs[static_cast<std::size_t>(i)].source);
    }
}
void DigitalLogicView::invalidate() {
    const QSignalBlocker block_gates(gates_), block_outputs(outputs_);
    for (int row = 0; row < gates_->rowCount(); ++row)
        gates_->item(row, 3)->setText("—");
    for (int row = 0; row < outputs_->rowCount(); ++row)
        outputs_->item(row, 2)->setText("—");
    truth_->setRowCount(0);
    truth_->setColumnCount(0);
    status_->setText("Draft or inputs changed. Evaluate or generate a truth table to validate.");
}
void DigitalLogicView::evaluate(bool table_requested) {
    invalidate();
    try {
        const Circuit circuit(draft_);
        const auto result = circuit.evaluate(assignments_);
        const QSignalBlocker block_gates(gates_), block_outputs(outputs_);
        for (int i = 0; i < gates_->rowCount(); ++i)
            gates_->item(i, 3)->setText(
                value_text(result.gate_values[static_cast<std::size_t>(i)]));
        for (int i = 0; i < outputs_->rowCount(); ++i)
            outputs_->item(i, 2)->setText(value_text(result.outputs[static_cast<std::size_t>(i)]));
        if (table_requested) {
            const auto generated = truth_table(circuit);
            QStringList headers;
            for (const auto& name : generated.input_names)
                headers.push_back("In: " + QString::fromStdString(name));
            for (const auto& name : generated.output_names)
                headers.push_back("Out: " + QString::fromStdString(name));
            truth_->setColumnCount(static_cast<int>(headers.size()));
            truth_->setHorizontalHeaderLabels(headers);
            truth_->setRowCount(static_cast<int>(generated.rows.size()));
            for (std::size_t row = 0; row < generated.rows.size(); ++row) {
                int column = 0;
                for (auto value : generated.rows[row].inputs)
                    truth_->setItem(static_cast<int>(row), column++, cell(value_text(value)));
                for (auto value : generated.rows[row].outputs)
                    truth_->setItem(static_cast<int>(row), column++, cell(value_text(value)));
            }
        }
        status_->setText(
            table_requested
                ? "Valid circuit. Truth table generated; current input values preserved."
                : "Valid circuit. Settled output values shown (no timing simulation).");
    } catch (const std::exception& error) {
        invalidate();
        status_->setText("Cannot evaluate draft: " + QString::fromUtf8(error.what()));
    }
}
} // namespace openece::gui
