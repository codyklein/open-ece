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
QString kind_name(const std::string& kind) { return qt_text(kind).toUpper(); }
project::Reference reference(unsigned id) {
    return id ? project::Reference{project::Id{id}} : std::nullopt;
}
unsigned node_id(project::Reference source) { return source ? source->value : 0; }
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

DigitalLogicView::DigitalLogicView(QWidget* parent, project::CombinationalDraft* draft, bool inert)
    : DraftView(parent), state_(draft, project::default_project().digital.combinational),
      draft_(state_.get()) {
    setObjectName("digital_logic_view");
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
    pin_count_ = new DraftInt(editor);
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
        "10 inputs / 1024 rows. Timing and storage live in the separate Timing / Sequential tab. "
        "No saving, constants, buses, Unknown/High-Z or minimization.</p>");
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
        edited();
    });
    connect(outputs_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (refreshing_ || item->column() != 0)
            return;
        draft_.outputs[static_cast<std::size_t>(item->row())].name =
            item->text().toUtf8().toStdString();
        invalidate();
        edited();
    });
    connect(gates_, &QTableWidget::currentCellChanged, this, [this] {
        if (!refreshing_)
            refresh_gate_editor();
    });
    connect(kind_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const int row = gates_->currentRow();
        if (refreshing_ || row < 0 || index < 0)
            return;
        draft_.gates[static_cast<std::size_t>(row)].kind = draft_text(kind_names[index].toLower());
        update_gate_row(row);
        invalidate();
        refresh_sources();
        edited();
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
        refresh();
        edited();
    });
    connect(remove_input, &QPushButton::clicked, this, [this] {
        const int row = inputs_->currentRow();
        if (row < 0)
            return;
        draft_.inputs.erase(draft_.inputs.begin() + row);
        refresh();
        edited();
    });
    connect(add_gate, &QPushButton::clicked, this, [this] {
        if (draft_.gates.size() >= digital_limits::gates) {
            status_->setText(QString("Gate limit reached (%1).").arg(digital_limits::gates));
            return;
        }
        const auto id = new_id();
        if (id.value == 0)
            return;
        draft_.gates.push_back({id, "and", {std::nullopt, std::nullopt}, "2"});
        refresh();
        edited();
        gates_->selectRow(static_cast<int>(draft_.gates.size()) - 1);
    });
    connect(remove_gate, &QPushButton::clicked, this, [this] {
        const int row = gates_->currentRow();
        if (row < 0)
            return;
        draft_.gates.erase(draft_.gates.begin() + row);
        refresh();
        edited();
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
        draft_.outputs.push_back({"Output " + std::to_string(id.value), std::nullopt});
        refresh();
        edited();
    });
    connect(remove_output, &QPushButton::clicked, this, [this] {
        const int row = outputs_->currentRow();
        if (row < 0)
            return;
        draft_.outputs.erase(draft_.outputs.begin() + row);
        refresh();
        edited();
    });
    bind_table(inputs_, [this](int row, int column, const QString& text) {
        if (column == 1 && row >= 0 && row < static_cast<int>(draft_.inputs.size())) {
            auto& name = draft_.inputs[static_cast<std::size_t>(row)].name;
            if (name != draft_text(text)) {
                edit(name, draft_text(text));
                invalidate();
                refresh_sources();
            }
        }
    });
    bind_table(outputs_, [this](int row, int column, const QString& text) {
        if (column == 0 && row >= 0 && row < static_cast<int>(draft_.outputs.size())) {
            auto& name = draft_.outputs[static_cast<std::size_t>(row)].name;
            if (name != draft_text(text)) {
                edit(name, draft_text(text));
                invalidate();
            }
        }
    });
    connect(pin_count_, &QSpinBox::valueChanged, this, [this](int count) {
        const int row = gates_->currentRow();
        if (restoring_ || refreshing_ || row < 0)
            return;
        auto& gate = draft_.gates[static_cast<std::size_t>(row)];
        edit(gate.pin_count_text, draft_text(static_cast<DraftInt*>(pin_count_)->raw_text()));
        if (static_cast<std::size_t>(count) != gate.pins.size()) {
            gate.pins.resize(static_cast<std::size_t>(count));
            update_gate_row(row);
            refresh_gate_editor();
            edited();
        }
        invalidate();
    });
    connect(static_cast<DraftInt*>(pin_count_)->editor(), &QLineEdit::textChanged, this, [this] {
        const int row = gates_->currentRow();
        if (restoring_ || refreshing_ || row < 0)
            return;
        auto& gate = draft_.gates[static_cast<std::size_t>(row)];
        const auto raw = static_cast<DraftInt*>(pin_count_)->raw_text();
        edit(gate.pin_count_text, draft_text(raw));
        bool ok = false;
        const int count = raw.toInt(&ok);
        if (ok && count >= 1 && count <= digital_limits::pins &&
            static_cast<std::size_t>(count) != gate.pins.size()) {
            gate.pins.resize(static_cast<std::size_t>(count));
            update_gate_row(row);
            refresh_gate_editor();
            edited();
        }
        invalidate();
    });
    refresh();
    restoring_ = false;
    if (!inert)
        evaluate(false);
    else
        status_->clear();
}

project::Id DigitalLogicView::new_id() {
    try {
        return project::allocate_id(draft_.next_id, project::reserved_ids(draft_));
    } catch (const std::exception& e) {
        status_->setText(QString::fromUtf8(e.what()));
        return {0};
    }
}
void DigitalLogicView::synchronize_pending_text() {
    DraftView::synchronize_pending_text();
    const int row = gates_->currentRow();
    if (row >= 0)
        edit(draft_.gates[static_cast<std::size_t>(row)].pin_count_text,
             draft_text(static_cast<DraftInt*>(pin_count_)->raw_text()));
}
digital::Circuit DigitalLogicView::validated_circuit() const {
    digital::CircuitDefinition result;
    for (const auto& input : draft_.inputs)
        result.inputs.push_back({{input.id.value}, input.name});
    for (const auto& gate : draft_.gates) {
        bool ok = false;
        const auto count = qt_text(gate.pin_count_text).toUInt(&ok);
        if (!ok || count != gate.pins.size())
            throw std::invalid_argument(
                "Invalid pending pin count; set the intended number of connections.");
        const int index = static_cast<int>(kind_names.indexOf(kind_name(gate.kind)));
        digital::Gate g{{gate.id.value}, kinds.at(static_cast<std::size_t>(index)), {}};
        for (auto pin : gate.pins)
            g.inputs.push_back({node_id(pin)});
        result.gates.push_back(std::move(g));
    }
    for (const auto& output : draft_.outputs)
        result.outputs.push_back({output.name, {node_id(output.source)}});
    return digital::Circuit(std::move(result));
}
std::vector<digital::LogicValue> DigitalLogicView::input_values() const {
    std::vector<digital::LogicValue> values;
    for (const auto& input : draft_.inputs)
        values.push_back(input.high ? LogicValue::one : LogicValue::zero);
    return values;
}
void DigitalLogicView::populate_sources(QComboBox* selector, project::Reference source) {
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
    int index = selector->findData(node_id(source));
    if (index < 0) {
        selector->addItem(QString("Missing node %1").arg(node_id(source)), node_id(source));
        index = selector->count() - 1;
    }
    selector->setCurrentIndex(index);
}
QComboBox* DigitalLogicView::source_selector(project::Reference source, const QString& name,
                                             QWidget* parent) {
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
        auto* toggle = new QCheckBox(draft_.inputs[index].high ? "1" : "0", inputs_);
        toggle->setObjectName(QString("digital_input_%1").arg(draft_.inputs[index].id.value));
        toggle->setChecked(draft_.inputs[index].high);
        inputs_->setCellWidget(i, 2, toggle);
        connect(toggle, &QCheckBox::toggled, this, [this, index, toggle](bool high) {
            edit(draft_.inputs[index].high, high);
            toggle->setText(high ? "1" : "0");
            invalidate();
            edited();
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
            draft_.outputs[index].source = reference(selector->currentData().toUInt());
            invalidate();
            edited();
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
    for (auto source : gate.pins) {
        bool found = false;
        for (const auto& input : draft_.inputs)
            found = found || project::Reference{input.id} == source;
        for (const auto& candidate : draft_.gates)
            found = found || project::Reference{candidate.id} == source;
        sources.push_back(node_id(source) == 0
                              ? "unconnected"
                              : (found ? QString::number(node_id(source))
                                       : QString("missing %1").arg(node_id(source))));
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
    const QSignalBlocker block_text(static_cast<DraftInt*>(pin_count_)->editor());
    pin_count_->setValue(static_cast<int>(gate.pins.size()));
    static_cast<DraftInt*>(pin_count_)->restore_text(gate.pin_count_text);
    for (std::size_t pin = 0; pin < gate.pins.size(); ++pin) {
        auto* selector = source_selector(gate.pins[pin], QString("digital_pin_%1").arg(pin),
                                         kind_->parentWidget());
        pin_form_->addRow(QString("Pin %1").arg(pin + 1), selector);
        connect(selector, &QComboBox::currentIndexChanged, this, [this, index, pin, selector] {
            draft_.gates[index].pins[pin] = reference(selector->currentData().toUInt());
            update_gate_row(static_cast<int>(index));
            invalidate();
            edited();
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
    synchronize_pending_text();
    invalidate();
    try {
        const Circuit circuit = validated_circuit();
        const auto result = circuit.evaluate(input_values());
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
