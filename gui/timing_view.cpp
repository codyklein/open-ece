#include "timing_view.hpp"
#include "timing_diagram_widget.hpp"
#include <QComboBox>
#include <QElapsedTimer>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <limits>

namespace openece::gui {
namespace d = digital;
namespace t = digital::timing;
namespace {
const QStringList kinds{"NOT",  "AND",      "OR",      "NAND",       "NOR",        "XOR",
                        "XNOR", "SR latch", "D latch", "DFF rising", "DFF falling"};
constexpr std::array gate_kinds{d::GateKind::Not,  d::GateKind::And, d::GateKind::Or,
                                d::GateKind::Nand, d::GateKind::Nor, d::GateKind::Xor,
                                d::GateKind::Xnor};
QString text(QTableWidget* table, int row, int column) { return table->item(row, column)->text(); }
std::uint64_t integer(const QString& value, std::uint64_t maximum, const char* what) {
    const auto trimmed = value.trimmed();
    static const QRegularExpression digits("^[0-9]+$");
    bool ok = false;
    const auto result = trimmed.toULongLong(&ok, 10);
    if (!ok || !digits.match(trimmed).hasMatch() || result > maximum)
        throw std::invalid_argument(std::string(what) +
                                    " must be a bounded nonnegative decimal integer");
    return result;
}
d::NodeId id(const QString& value) {
    return {static_cast<std::uint32_t>(
        integer(value, std::numeric_limits<std::uint32_t>::max(), "Node ID"))};
}
d::LogicValue logic(const QString& value) {
    return integer(value, 1, "Logic value") ? d::LogicValue::one : d::LogicValue::zero;
}
std::vector<d::NodeId> ids(const QString& value, int limit) {
    if (value.size() > limit * 12)
        throw std::length_error("Node list exceeds GUI limit");
    if (value.trimmed().isEmpty())
        return {};
    const auto pieces = value.split(',');
    if (pieces.size() > limit)
        throw std::length_error("Too many node IDs");
    std::vector<d::NodeId> result;
    for (const auto& piece : pieces)
        result.push_back(id(piece));
    return result;
}
QPushButton* button(QHBoxLayout* row, const QString& label, const char* name) {
    auto* b = new QPushButton(label);
    b->setObjectName(name);
    row->addWidget(b);
    return b;
}
} // namespace
TimingView::TimingView(QWidget* parent) : QWidget(parent) {
    setObjectName("timing_view");
    auto* layout = new QVBoxLayout(this);
    auto* help = new QLabel(
        "Edit cells by double-clicking. Connect by stable node ID; pin lists use commas. "
        "All times/delays are integer picoseconds (1000 ps = 1 ns). Gates: pin list; SR: S,R; "
        "D latch: D,enable; DFF: D,clock. Initial Q applies only to storage. "
        "Clock first edge toggles the initial level; high/low are level durations. "
        "Leave all three clock cells blank for manual input.",
        this);
    help->setWordWrap(true);
    layout->addWidget(help);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName("timing_editor_tabs");
    auto make_table = [&](const char* name, const QString& title, const QStringList& headers,
                          int limit, bool node) {
        auto* page = new QWidget(tabs);
        auto* page_layout = new QVBoxLayout(page);
        auto* table = new QTableWidget(0, static_cast<int>(headers.size()), page);
        table->setObjectName(name);
        table->setHorizontalHeaderLabels(headers);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        page_layout->addWidget(table);
        auto* row = new QHBoxLayout;
        const auto add_name = QByteArray(name) + "_add", remove_name = QByteArray(name) + "_remove";
        auto* add = button(row, "Add row", add_name.constData());
        auto* remove = button(row, "Remove row", remove_name.constData());
        row->addStretch();
        page_layout->addLayout(row);
        tabs->addTab(page, title);
        connect(table, &QTableWidget::itemChanged, this, [this] { invalidate(); });
        connect(add, &QPushButton::clicked, this, [=, this] {
            if (table->rowCount() >= limit) {
                status_->setText("GUI row limit reached.");
                return;
            }
            try {
                QStringList values;
                for (int i = 0; i < table->columnCount(); ++i)
                    values.push_back("");
                if (node)
                    values[0] = QString::number(new_id());
                append(table, values, node);
                invalidate();
            } catch (const std::exception& e) {
                fail(e);
            }
        });
        connect(remove, &QPushButton::clicked, this, [=, this] {
            if (table->currentRow() >= 0) {
                table->removeRow(table->currentRow());
                invalidate();
            }
        });
        return table;
    };
    inputs_ = make_table("timing_inputs", "Inputs / clocks",
                         {"ID", "Name", "Initial 0/1", "First edge (ps)", "High (ps)", "Low (ps)"},
                         timing_gui_limits::inputs, true);
    elements_ = make_table("timing_elements", "Elements",
                           {"ID", "Type", "Pin IDs (ordered)", "Delay (ps)", "Initial Q"},
                           timing_gui_limits::elements, true);
    outputs_ = make_table("timing_outputs", "Outputs", {"Name", "Source ID"},
                          timing_gui_limits::outputs, false);
    stimuli_ = make_table("timing_stimuli", "Input changes", {"Time (ps)", "Input ID", "Value 0/1"},
                          timing_gui_limits::stimuli, false);
    tabs->setMinimumHeight(220);
    layout->addWidget(tabs, 2);
    auto* settings = new QHBoxLayout;
    horizon_ = new QLineEdit("30000", this);
    horizon_->setObjectName("timing_horizon");
    horizon_->setMaxLength(13);
    observed_ = new QLineEdit("1,2,3", this);
    observed_->setObjectName("timing_observed");
    observed_->setMaxLength(timing_gui_limits::observed * 12);
    scale_ = new QComboBox(this);
    scale_->setObjectName("timing_scale");
    scale_->addItems({"ps", "ns", "µs", "ms", "s"});
    scale_->setCurrentIndex(1);
    settings->addWidget(new QLabel("End (ps):", this));
    settings->addWidget(horizon_);
    settings->addWidget(new QLabel("Observe IDs:", this));
    settings->addWidget(observed_, 2);
    settings->addWidget(new QLabel("Display:", this));
    settings->addWidget(scale_);
    layout->addLayout(settings);
    auto* actions = new QHBoxLayout;
    auto* run = button(actions, "Run / resume", "timing_run");
    auto* pause = button(actions, "Pause", "timing_pause");
    auto* step = button(actions, "Step timestamp", "timing_step");
    auto* reset = button(actions, "Reset", "timing_reset");
    auto* example_button = button(actions, "Load DFF example", "timing_example");
    actions->addStretch();
    layout->addLayout(actions);
    status_ = new QLabel(this);
    status_->setObjectName("timing_status");
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    results_ = new QTableWidget(0, 2, this);
    results_->setObjectName("timing_results");
    results_->setHorizontalHeaderLabels({"Output", "Visible value"});
    results_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    results_->setMaximumHeight(95);
    layout->addWidget(results_);
    diagram_ = new TimingDiagramWidget(this);
    layout->addWidget(diagram_, 3);
    auto* conventions = new QLabel(
        "Gates suppress pulses shorter than their delay; exact-delay pulses survive. "
        "Storage captures are delivered in order. DFFs sample pre-batch D; a closing D latch holds "
        "pre-batch D. "
        "SR=11 stops the run. Initial gates are settled; timing traces show visible Q, without "
        "delay compensation. "
        "Edits/reset discard the session. No power-up, setup/hold or metastability model.",
        this);
    conventions->setWordWrap(true);
    layout->addWidget(conventions);
    timer_ = new QTimer(this);
    timer_->setInterval(0);
    connect(timer_, &QTimer::timeout, this, [this] { advance(true); });
    connect(run, &QPushButton::clicked, this, [this] {
        try {
            if (!simulation_)
                initialize();
            if (simulation_->status() != t::StepStatus::Complete) {
                timer_->start();
                status_->setText("Running…");
            }
        } catch (const std::exception& e) {
            fail(e);
        }
    });
    connect(pause, &QPushButton::clicked, this, [this] {
        timer_->stop();
        if (simulation_)
            render();
    });
    connect(step, &QPushButton::clicked, this, [this] {
        timer_->stop();
        advance(false);
    });
    connect(reset, &QPushButton::clicked, this, [this] { invalidate(); });
    connect(example_button, &QPushButton::clicked, this, [this] { example(); });
    connect(horizon_, &QLineEdit::textChanged, this, [this] { invalidate(); });
    connect(observed_, &QLineEdit::textChanged, this, [this] { invalidate(); });
    connect(scale_, &QComboBox::currentIndexChanged, this, [this] {
        if (simulation_)
            render();
    });
    example();
}
void TimingView::append(QTableWidget* table, const QStringList& values, bool node) {
    const QSignalBlocker block(table);
    const auto row = table->rowCount();
    table->insertRow(row);
    for (int i = 0; i < table->columnCount(); ++i) {
        auto* cell = new QTableWidgetItem(values[i]);
        if (node && i == 0)
            cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, i, cell);
    }
    if (table == elements_) {
        auto* type = new QComboBox(table);
        type->addItems(kinds);
        type->setCurrentIndex(values[1].isEmpty() ? 0 : static_cast<int>(kinds.indexOf(values[1])));
        table->setCellWidget(row, 1, type);
        connect(type, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });
    }
    table->selectRow(row);
}
std::uint32_t TimingView::new_id() {
    if (next_id_ > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Node ID space exhausted");
    return static_cast<std::uint32_t>(next_id_++);
}
void TimingView::invalidate() {
    timer_->stop();
    simulation_.reset();
    results_->setRowCount(0);
    diagram_->clear();
    status_->setText("Draft changed. Run or Step to validate and start a new session.");
}
void TimingView::fail(const std::exception& error) {
    invalidate();
    status_->setText("Cannot simulate: " + QString::fromUtf8(error.what()));
}
void TimingView::example() {
    invalidate();
    for (auto* table : {inputs_, elements_, outputs_, stimuli_})
        table->setRowCount(0);
    append(inputs_, {"1", "D", "0", "", "", ""}, true);
    append(inputs_, {"2", "CLK", "0", "5000", "5000", "5000"}, true);
    append(elements_, {"3", "DFF rising", "1,2", "1000", "0"}, true);
    append(outputs_, {"Q", "3"});
    append(stimuli_, {"2000", "1", "1"});
    append(stimuli_, {"12000", "1", "0"});
    next_id_ = 4;
    horizon_->setText("30000");
    observed_->setText("1,2,3");
    status_->setText("DFF example ready: Q rises at 6000 ps and falls at 16000 ps. Run or Step.");
}
void TimingView::initialize() {
    t::TimedCircuitDefinition def;
    t::SimulationRequest request;
    request.horizon = {integer(horizon_->text(), t::limits::time_ticks, "End time")};
    request.observed = ids(observed_->text(), timing_gui_limits::observed);
    request.work = {timing_gui_limits::queued, timing_gui_limits::processed,
                    timing_gui_limits::pin_visits, timing_gui_limits::recorded};
    for (int row = 0; row < inputs_->rowCount(); ++row) {
        const auto node = id(text(inputs_, row, 0));
        def.inputs.push_back({node, text(inputs_, row, 1).toUtf8().toStdString()});
        request.initial_inputs.push_back(logic(text(inputs_, row, 2)));
        if (!text(inputs_, row, 3).trimmed().isEmpty() ||
            !text(inputs_, row, 4).trimmed().isEmpty() ||
            !text(inputs_, row, 5).trimmed().isEmpty())
            request.clocks.push_back(
                {node,
                 {integer(text(inputs_, row, 3), t::limits::time_ticks, "First edge")},
                 {integer(text(inputs_, row, 4), t::limits::time_ticks, "High duration")},
                 {integer(text(inputs_, row, 5), t::limits::time_ticks, "Low duration")}});
    }
    for (int row = 0; row < elements_->rowCount(); ++row) {
        const auto node = id(text(elements_, row, 0));
        const auto kind = static_cast<QComboBox*>(elements_->cellWidget(row, 1))->currentIndex();
        const auto pins = ids(text(elements_, row, 2), timing_gui_limits::pins);
        const t::Delay delay{
            integer(text(elements_, row, 3), t::limits::time_ticks, "Element delay")};
        if (kind < 0)
            throw std::invalid_argument("Select an element type");
        if (kind < 7) {
            if (!text(elements_, row, 4).trimmed().isEmpty())
                throw std::invalid_argument("Initial Q must be blank for gates");
            def.elements.emplace_back(
                t::DelayedGate{{node, gate_kinds[static_cast<std::size_t>(kind)], pins}, delay});
        } else {
            if (pins.size() != 2)
                throw std::invalid_argument("Storage requires two pins in the documented order");
            if (kind == 7)
                def.elements.emplace_back(t::SrLatch{node, pins[0], pins[1], delay});
            else if (kind == 8)
                def.elements.emplace_back(t::DLatch{node, pins[0], pins[1], delay});
            else
                def.elements.emplace_back(t::DFlipFlop{
                    node, pins[0], pins[1], kind == 9 ? t::Edge::Rising : t::Edge::Falling, delay});
            request.initial_storage.push_back({node, logic(text(elements_, row, 4))});
        }
    }
    output_names_.clear();
    trace_labels_.clear();
    for (int row = 0; row < outputs_->rowCount(); ++row) {
        def.outputs.push_back(
            {text(outputs_, row, 0).toUtf8().toStdString(), id(text(outputs_, row, 1))});
        output_names_.push_back(def.outputs.back().name);
    }
    for (int row = 0; row < stimuli_->rowCount(); ++row)
        request.changes.push_back(
            {{integer(text(stimuli_, row, 0), t::limits::time_ticks, "Stimulus time")},
             id(text(stimuli_, row, 1)),
             logic(text(stimuli_, row, 2))});
    for (auto node : request.observed) {
        QString label = "Node " + QString::number(node.value);
        for (const auto& input : def.inputs)
            if (input.id == node)
                label += " / " + QString::fromStdString(input.name);
        for (const auto& output : def.outputs)
            if (output.source == node)
                label += " / " + QString::fromStdString(output.name);
        trace_labels_.push_back(label);
    }
    simulation_ =
        std::make_unique<t::Simulation>(t::TimedCircuit(std::move(def)), std::move(request));
    render();
}
void TimingView::advance(bool run) {
    try {
        if (!simulation_)
            initialize();
        QElapsedTimer elapsed;
        elapsed.start();
        const int count = run ? timing_gui_limits::batch_timestamps : 1;
        for (int i = 0; i < count; ++i) {
            if (simulation_->step() == t::StepStatus::Complete) {
                timer_->stop();
                break;
            }
            if (elapsed.elapsed() >= 8)
                break;
        }
        render();
    } catch (const std::exception& error) {
        fail(error);
    }
}
void TimingView::render() {
    const auto snapshot = simulation_->snapshot();
    results_->setRowCount(static_cast<int>(snapshot.outputs.size()));
    for (std::size_t i = 0; i < snapshot.outputs.size(); ++i) {
        results_->setItem(static_cast<int>(i), 0,
                          new QTableWidgetItem(QString::fromStdString(output_names_[i])));
        results_->setItem(
            static_cast<int>(i), 1,
            new QTableWidgetItem(snapshot.outputs[i] == d::LogicValue::one ? "1" : "0"));
    }
    constexpr std::array<double, 5> units{1, 1000, 1'000'000, 1'000'000'000, 1'000'000'000'000};
    diagram_->present(snapshot, trace_labels_,
                      units[static_cast<std::size_t>(scale_->currentIndex())],
                      scale_->currentText());
    status_->setText(QString("%1 at %2 ps. Visible outputs shown; %3 observed nodes.")
                         .arg(snapshot.status == t::StepStatus::Complete ? "Complete"
                              : timer_->isActive()                       ? "Running"
                                                                         : "Paused")
                         .arg(static_cast<qulonglong>(snapshot.reached.ticks))
                         .arg(snapshot.traces.size()));
}
void TimingView::load_combinational(const d::Circuit& circuit,
                                    const std::vector<d::LogicValue>& inputs, t::Delay delay) {
    const auto def = t::with_delay(circuit, delay);
    if (def.inputs.size() > timing_gui_limits::inputs ||
        def.elements.size() > timing_gui_limits::elements ||
        def.outputs.size() > timing_gui_limits::outputs || inputs.size() != def.inputs.size())
        throw std::length_error("Circuit exceeds timing GUI limits");
    for (const auto& element : def.elements)
        if (t::element_inputs(element).size() > timing_gui_limits::pins)
            throw std::length_error("Pin count exceeds timing GUI limit");
    invalidate();
    for (auto* table : {inputs_, elements_, outputs_, stimuli_})
        table->setRowCount(0);
    next_id_ = 1;
    QStringList observed;
    auto note = [&](d::NodeId node) {
        next_id_ = std::max(next_id_, static_cast<std::uint64_t>(node.value) + 1);
        if (observed.size() < timing_gui_limits::observed)
            observed.push_back(QString::number(node.value));
    };
    for (std::size_t i = 0; i < def.inputs.size(); ++i) {
        const auto& input = def.inputs[i];
        note(input.id);
        append(inputs_,
               {QString::number(input.id.value), QString::fromStdString(input.name),
                inputs[i] == d::LogicValue::one ? "1" : "0", "", "", ""},
               true);
    }
    for (const auto& element : def.elements) {
        const auto& gate = std::get<t::DelayedGate>(element).gate;
        note(gate.id);
        QStringList pins;
        for (auto pin : gate.inputs)
            pins.push_back(QString::number(pin.value));
        const auto index = static_cast<int>(
            std::find(gate_kinds.begin(), gate_kinds.end(), gate.kind) - gate_kinds.begin());
        append(elements_,
               {QString::number(gate.id.value), kinds[index], pins.join(','),
                QString::number(static_cast<qulonglong>(delay.ticks)), ""},
               true);
    }
    for (const auto& output : def.outputs)
        append(outputs_,
               {QString::fromStdString(output.name), QString::number(output.source.value)});
    observed_->setText(observed.join(','));
    status_->setText("Independent copy loaded. Add input changes or clocks, then Run. First 16 "
                     "nodes selected for observation at most.");
}
} // namespace openece::gui
