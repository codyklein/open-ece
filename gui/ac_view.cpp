#include "ac_view.hpp"
#include "ac_response_plot.hpp"
#include "circuit_value.hpp"
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
namespace openece::gui {
namespace ac = circuits::ac;
namespace {
QTableWidget* table(const char* name, const QStringList& columns, QWidget* parent,
                    bool editable = true) {
    auto* t = new QTableWidget(0, static_cast<int>(columns.size()), parent);
    t->setObjectName(name);
    t->setHorizontalHeaderLabels(columns);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->verticalHeader()->hide();
    if (!editable)
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    return t;
}
QTableWidgetItem* fixed_item(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}
QComboBox* combo(QTableWidget* t, int row, int column) {
    return static_cast<QComboBox*>(t->cellWidget(row, column));
}
QLineEdit* line(QTableWidget* t, int row, int column) {
    return static_cast<QLineEdit*>(t->cellWidget(row, column));
}
QString number(double value) { return QString::number(value, 'g', 12); }
QString diagnostic(const circuits::CircuitError& e) {
    QString text = QString::fromUtf8(e.what());
    for (auto id : e.nodes())
        text += " Node ID " + QString::number(id.value) + ".";
    for (auto id : e.components())
        text += " Component ID " + QString::number(id.value) + ".";
    return text;
}
void units(QComboBox* unit, int kind) {
    const QSignalBlocker block(unit);
    unit->clear();
    if (kind == 0) {
        unit->addItem(QString::fromUtf8("Ω"), 1.);
        unit->addItem(QString::fromUtf8("kΩ"), 1e3);
        unit->addItem(QString::fromUtf8("MΩ"), 1e6);
    } else if (kind == 1) {
        unit->addItem("F", 1.);
        unit->addItem(QString::fromUtf8("µF"), 1e-6);
        unit->addItem("nF", 1e-9);
        unit->addItem("pF", 1e-12);
    } else if (kind == 2) {
        unit->addItem("H", 1.);
        unit->addItem("mH", 1e-3);
        unit->addItem(QString::fromUtf8("µH"), 1e-6);
    } else {
        const QString base = kind == 3 ? "V" : "A";
        unit->addItem(base, 1.);
        unit->addItem("m" + base, 1e-3);
        unit->addItem(QString::fromUtf8("µ") + base, 1e-6);
    }
    unit->setProperty("previousUnit", 0);
}
circuits::NodeId selected_node(QComboBox* selector) {
    if (!selector->currentData().isValid())
        throw std::invalid_argument("Select a node for every required connection and probe.");
    return {selector->currentData().toUInt()};
}
double physical_value(QLineEdit* text, QComboBox* unit) {
    const auto value = circuit_value_si(text->text(), unit->currentData().toDouble());
    if (!value)
        throw std::invalid_argument(
            "Invalid numeric value; correct the visible text before calculating.");
    return *value;
}
void phasor_cells(QTableWidget* t, int row, const QString& name, ac::Phasor value) {
    t->setItem(row, 0, fixed_item(name));
    t->setItem(row, 1, fixed_item(number(value.real())));
    t->setItem(row, 2, fixed_item(number(value.imag())));
    t->setItem(row, 3, fixed_item(number(std::abs(value))));
    const auto phase = ac::wrapped_phase_degrees(value);
    t->setItem(row, 4, fixed_item(phase ? number(*phase) : QString::fromUtf8("—")));
}
} // namespace
AcView::AcView(QWidget* parent) : QWidget(parent) {
    loading_ = true;
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(
        new QLabel("Sinusoidal steady-state AC — RMS phasors, cosine reference", this));
    tabs_ = new QTabWidget(this);
    tabs_->setObjectName("ac_view_tabs");
    layout->addWidget(tabs_);
    auto* editor = new QWidget(tabs_);
    tabs_->addTab(editor, "Circuit");
    auto* edit_layout = new QVBoxLayout(editor);
    auto* tables = new QHBoxLayout;
    edit_layout->addLayout(tables);
    auto* node_layout = new QVBoxLayout;
    tables->addLayout(node_layout, 1);
    nodes_ = table("ac_nodes", {"ID", "Name"}, editor);
    node_layout->addWidget(nodes_);
    ground_ = new QComboBox(editor);
    ground_->setObjectName("ac_ground");
    ground_->setAccessibleName("AC ground node");
    node_layout->addWidget(new QLabel("Ground (0 V)", editor));
    node_layout->addWidget(ground_);
    auto* node_actions = new QHBoxLayout;
    node_layout->addLayout(node_actions);
    auto button = [&](const char* name, const QString& text, QHBoxLayout* row, auto action) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(name);
        row->addWidget(b);
        connect(b, &QPushButton::clicked, this, action);
        return b;
    };
    button("ac_add_node", "Add node", node_actions, [this] { add_node(); });
    button("ac_remove_node", "Remove", node_actions, [this] {
        if (nodes_->currentRow() >= 0) {
            nodes_->removeRow(nodes_->currentRow());
            refresh_connections();
            invalidate();
        }
    });
    auto* part_layout = new QVBoxLayout;
    tables->addLayout(part_layout, 3);
    components_ = table("ac_components",
                        {"ID", "Name", "Type", "Positive (+)", "Negative (−)",
                         "Value / RMS magnitude", "Unit", "Phase (°)"},
                        editor);
    part_layout->addWidget(components_);
    auto* part_actions = new QHBoxLayout;
    part_layout->addLayout(part_actions);
    button("ac_add_component", "Add component", part_actions, [this] { add_component(); });
    button("ac_remove_component", "Remove", part_actions, [this] {
        if (components_->currentRow() >= 0) {
            components_->removeRow(components_->currentRow());
            refresh_sources();
            invalidate();
        }
    });
    button("ac_example_rc", "RC low-pass", part_actions, [this] { load_example(false); });
    button("ac_example_rlc", "Series RLC", part_actions, [this] { load_example(true); });
    auto* single = new QWidget(tabs_);
    tabs_->addTab(single, "Single-frequency results");
    auto* single_layout = new QVBoxLayout(single);
    voltages_ =
        table("ac_voltages",
              {"Node", "Real (V RMS)", "Imaginary (V RMS)", "Magnitude (V RMS)", "Phase (°)"},
              single, false);
    currents_ = table(
        "ac_currents",
        {"Voltage source", "Real (A RMS)", "Imaginary (A RMS)", "Magnitude (A RMS)", "Phase (°)"},
        single, false);
    single_layout->addWidget(voltages_);
    single_layout->addWidget(currents_);
    auto* sweep_page = new QWidget(tabs_);
    tabs_->addTab(sweep_page, "Frequency sweep");
    auto* sweep_layout = new QVBoxLayout(sweep_page);
    auto frequency_controls = [&](const char* name, const char* unit_name, const QString& label,
                                  QLineEdit*& value, QComboBox*& unit, QHBoxLayout* row) {
        row->addWidget(new QLabel(label, this));
        value = new QLineEdit("1", this);
        value->setObjectName(name);
        value->setMaxLength(128);
        value->setMaximumWidth(150);
        row->addWidget(value);
        unit = new QComboBox(this);
        unit->setObjectName(unit_name);
        unit->addItem("Hz", 1.);
        unit->addItem("kHz", 1e3);
        unit->addItem("MHz", 1e6);
        unit->addItem("GHz", 1e9);
        unit->setProperty("previousUnit", 0);
        row->addWidget(unit);
        connect(value, &QLineEdit::textChanged, this, [this] { invalidate(); });
        connect_units(unit, value);
    };
    auto* sweep_controls = new QHBoxLayout;
    sweep_layout->addLayout(sweep_controls);
    frequency_controls("ac_start", "ac_start_unit", "Start", start_, start_unit_, sweep_controls);
    frequency_controls("ac_stop", "ac_stop_unit", "Stop", stop_, stop_unit_, sweep_controls);
    spacing_ = new QComboBox(this);
    spacing_->setObjectName("ac_spacing");
    spacing_->addItems({"Logarithmic", "Linear"});
    sweep_controls->addWidget(spacing_);
    count_ = new QSpinBox(this);
    count_->setObjectName("ac_point_count");
    count_->setRange(2, ac_gui_limits::sweep_points);
    count_->setValue(201);
    count_->setSuffix(" points");
    sweep_controls->addWidget(count_);
    auto* outputs = new QHBoxLayout;
    sweep_layout->addLayout(outputs);
    auto selector = [&](const char* name, const QString& label) {
        outputs->addWidget(new QLabel(label, this));
        auto* c = new QComboBox(this);
        c->setObjectName(name);
        outputs->addWidget(c);
        connect(c, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });
        return c;
    };
    probe_positive_ = selector("ac_probe_positive", "Output +");
    probe_negative_ = selector("ac_probe_negative", "Output −");
    mode_ = selector("ac_response_mode", "Response");
    mode_->addItems({"Voltage transfer (dB)", "Absolute voltage (V RMS)"});
    reference_ = selector("ac_reference_source", "Reference source");
    connect(mode_, &QComboBox::currentIndexChanged, this,
            [this] { reference_->setEnabled(mode_->currentIndex() == 0); });
    auto* sweep_actions = new QHBoxLayout;
    sweep_layout->addLayout(sweep_actions);
    button("ac_run_sweep", "Run sweep", sweep_actions, [this] { start_sweep(); });
    cancel_ = button("ac_cancel", "Cancel", sweep_actions, [this] { cancel_sweep(); });
    cancel_->setEnabled(false);
    sweep_actions->addStretch();
    auto* plots = new QHBoxLayout;
    sweep_layout->addLayout(plots, 2);
    magnitude_plot_ = new AcResponsePlot(false, this);
    magnitude_plot_->setObjectName("ac_magnitude_plot");
    plots->addWidget(magnitude_plot_);
    phase_plot_ = new AcResponsePlot(true, this);
    phase_plot_->setObjectName("ac_phase_plot");
    plots->addWidget(phase_plot_);
    sweep_ = table("ac_sweep_results",
                   {"Frequency (Hz)", "Real", "Imaginary", "Magnitude", "Phase (°)", "Status"},
                   this, false);
    sweep_layout->addWidget(sweep_, 1);
    auto* help = new QTextBrowser(this);
    tabs_->addTab(help, "Conventions");
    help->setHtml(QString::fromUtf8(
        "<h2>AC phasors</h2><p>RMS cosine reference: x(t) = √2 Re{X exp(j2πft)}. "
        "Sources share one positive analysis frequency. There is no DC bias or transient model.</p>"
        "<p>Voltage is V(+) − V(−); branch current is + → −. Source magnitude is nonnegative; "
        "use phase for polarity. Phase input is in degrees. Zero response has undefined phase.</p>"
        "<p>Choose R, C, L or independent voltage/current sources. Units convert physical values. "
        "Invalid pending text remains visible. Missing connections are never repaired "
        "automatically.</p>"
        "<p>Voltage-transfer mode divides the measured voltage by the selected nonzero "
        "voltage-source "
        "phasor. Every other voltage AND current source must be zero. Absolute-voltage mode needs "
        "no reference and reports V RMS. Gain uses 20 log10|H| with a −240 dB display floor; "
        "phase is wrapped to (−180°,180°] and hidden at/below that gain floor.</p>"
        "<p>Every requested sweep frequency stays in the results table. Failed points are gaps, "
        "never zero or interpolated values. Cancellation retains completed points and explicitly "
        "marks remaining frequencies as not evaluated; it is not a completed sweep.</p>"
        "<p>Edits cancel an active sweep and clear stale results. Switching domains preserves "
        "drafts "
        "and results. A sweep owns a validated snapshot and yields between frequency solves.</p>"
        "<p>Limits: 32 nodes, 128 components, 32 voltage sources, 1001 sweep points; the core's "
        "aggregate work limit also applies. Frequencies: 1e−6…1e12 Hz. C: 1e−15…1 F; "
        "L: 1e−12…1e6 H; R: 1e−9…1e12 Ω. Source magnitude: zero or 1e−12…1e9 V/A RMS.</p>"
        "<p>Floating, contradictory, non-unique and numerically unreliable networks fail "
        "explicitly. "
        "Ideal resonance is never regularized. Near-resonant ideal results do not model real "
        "losses. "
        "The RC example measures across the capacitor: its corner is −3.0103 dB and −45°.</p>"));
    auto* single_controls = new QHBoxLayout;
    layout->addLayout(single_controls);
    frequency_controls("ac_frequency", "ac_frequency_unit", "Single frequency", frequency_,
                       frequency_unit_, single_controls);
    button("ac_solve", "Solve AC", single_controls, [this] { solve(); });
    single_controls->addStretch();
    status_ = new QLabel(this);
    status_->setObjectName("ac_status");
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(status_);
    timer_ = new QTimer(this);
    timer_->setInterval(0);
    connect(timer_, &QTimer::timeout, this, [this] { advance_sweep(); });
    connect(nodes_, &QTableWidget::itemChanged, this, [this] {
        if (!loading_) {
            refresh_connections();
            invalidate();
        }
    });
    connect(components_, &QTableWidget::itemChanged, this, [this] {
        if (!loading_) {
            refresh_sources();
            invalidate();
        }
    });
    connect(ground_, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });
    connect(spacing_, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });
    connect(count_, &QSpinBox::valueChanged, this, [this] { invalidate(); });
    load_example(false);
}
void AcView::connect_units(QComboBox* unit, QLineEdit* value) {
    connect(unit, &QComboBox::currentIndexChanged, this, [this, unit, value](int selected) {
        const int previous = unit->property("previousUnit").toInt();
        if (previous == selected)
            return;
        const auto si = circuit_value_si(value->text(), unit->itemData(previous).toDouble());
        const double converted = si ? *si / unit->itemData(selected).toDouble() : 0;
        if (!si || !std::isfinite(converted) || (*si != 0 && converted == 0)) {
            const QSignalBlocker block(unit);
            unit->setCurrentIndex(previous);
            invalidate();
            if (status_)
                status_->setText("Invalid pending value. Correct it before changing units.");
            return;
        }
        unit->setProperty("previousUnit", selected);
        value->setText(QString::number(converted, 'g', 17));
        invalidate();
    });
}
void AcView::invalidate() {
    if (loading_)
        return;
    timer_->stop();
    cancel_->setEnabled(false);
    run_circuit_.reset();
    frequencies_.clear();
    points_.clear();
    magnitudes_.clear();
    phases_.clear();
    failures_ = 0;
    voltages_->setRowCount(0);
    currents_->setRowCount(0);
    sweep_->setRowCount(0);
    magnitude_plot_->clear();
    phase_plot_->clear();
    status_->setText("Draft changed. Solve AC or run a sweep to validate it.");
}
void AcView::add_node(const QString& name) {
    if (nodes_->rowCount() >= ac_gui_limits::nodes ||
        next_node_ > std::numeric_limits<std::uint32_t>::max()) {
        status_->setText("Node limit reached.");
        return;
    }
    const QSignalBlocker block(nodes_);
    int row = nodes_->rowCount();
    nodes_->insertRow(row);
    auto id = static_cast<unsigned>(next_node_++);
    auto* item = fixed_item(QString::number(id));
    item->setData(Qt::UserRole, id);
    nodes_->setItem(row, 0, item);
    nodes_->setItem(row, 1,
                    new QTableWidgetItem(name.isEmpty() ? "N" + QString::number(id) : name));
    refresh_connections();
    invalidate();
}
void AcView::refresh_connection(QComboBox* selector) {
    const auto old = selector->currentData();
    QSignalBlocker block(selector);
    selector->clear();
    selector->addItem("Select node");
    for (int i = 0; i < nodes_->rowCount(); ++i)
        selector->addItem(nodes_->item(i, 1)->text() + " [" + nodes_->item(i, 0)->text() + "]",
                          nodes_->item(i, 0)->data(Qt::UserRole));
    if (old.isValid()) {
        int index = selector->findData(old);
        if (index < 0) {
            selector->addItem("Missing node " + old.toString(), old);
            index = selector->count() - 1;
        }
        selector->setCurrentIndex(index);
    }
}
void AcView::refresh_connections() {
    for (auto* selector : {ground_, probe_positive_, probe_negative_})
        refresh_connection(selector);
    for (int row = 0; row < components_->rowCount(); ++row)
        for (int column : {3, 4})
            refresh_connection(combo(components_, row, column));
}
void AcView::refresh_sources() {
    const auto old = reference_->currentData();
    const QSignalBlocker block(reference_);
    reference_->clear();
    reference_->addItem("Select voltage source");
    for (int row = 0; row < components_->rowCount(); ++row)
        if (combo(components_, row, 2)->currentIndex() == 3)
            reference_->addItem(components_->item(row, 1)->text() + " [" +
                                    components_->item(row, 0)->text() + "]",
                                components_->item(row, 0)->data(Qt::UserRole));
    if (old.isValid()) {
        auto index = reference_->findData(old);
        if (index < 0) {
            reference_->addItem("Missing voltage source " + old.toString(), old);
            index = reference_->count() - 1;
        }
        reference_->setCurrentIndex(index);
    }
}
void AcView::add_component() {
    if (components_->rowCount() >= ac_gui_limits::components ||
        next_component_ > std::numeric_limits<std::uint32_t>::max()) {
        status_->setText("Component limit reached.");
        return;
    }
    const QSignalBlocker block(components_);
    const int row = components_->rowCount();
    components_->insertRow(row);
    const auto id = static_cast<unsigned>(next_component_++);
    auto* item = fixed_item(QString::number(id));
    item->setData(Qt::UserRole, id);
    components_->setItem(row, 0, item);
    components_->setItem(row, 1, new QTableWidgetItem("P" + QString::number(id)));
    auto* type = new QComboBox(components_);
    type->addItems({"Resistor", "Capacitor", "Inductor", "AC voltage source", "AC current source"});
    components_->setCellWidget(row, 2, type);
    for (int column : {3, 4}) {
        auto* selector = new QComboBox(components_);
        components_->setCellWidget(row, column, selector);
        refresh_connection(selector);
        connect(selector, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });
    }
    auto* value = new QLineEdit(components_);
    value->setMaxLength(128);
    value->setMinimumWidth(85);
    components_->setCellWidget(row, 5, value);
    auto* unit = new QComboBox(components_);
    units(unit, 0);
    components_->setCellWidget(row, 6, unit);
    auto* phase = new QLineEdit("0", components_);
    phase->setMaxLength(128);
    phase->setMaximumWidth(90);
    phase->setEnabled(false);
    components_->setCellWidget(row, 7, phase);
    connect(type, &QComboBox::currentIndexChanged, this, [this, unit, value, phase](int kind) {
        units(unit, kind);
        value->clear();
        phase->setText("0");
        phase->setEnabled(kind >= 3);
        refresh_sources();
        invalidate();
    });
    connect(value, &QLineEdit::textChanged, this, [this] { invalidate(); });
    connect(phase, &QLineEdit::textChanged, this, [this] { invalidate(); });
    connect_units(unit, value);
    refresh_sources();
    invalidate();
}
void AcView::load_example(bool rlc) {
    invalidate();
    loading_ = true;
    components_->setRowCount(0);
    nodes_->setRowCount(0);
    next_node_ = next_component_ = 0;
    ground_->clear();
    probe_positive_->clear();
    probe_negative_->clear();
    reference_->clear();
    add_node("Ground");
    add_node("Input");
    add_node(rlc ? "LC junction" : "Output");
    if (rlc)
        add_node("Output");
    ground_->setCurrentIndex(ground_->findData(0u));
    const int count = rlc ? 4 : 3;
    const int types[] = {3, rlc ? 2 : 0, 1, 0};
    const unsigned positive[] = {1, 1, 2, 3}, negative[] = {0, 2, rlc ? 3u : 0u, 0};
    const QString names[] = {"V1", rlc ? "L1" : "R1", "C1", "R1"};
    const QString values[] = {"1", rlc ? "0.01" : "1000", "1e-6", "100"};
    for (int row = 0; row < count; ++row) {
        add_component();
        combo(components_, row, 2)->setCurrentIndex(types[row]);
        components_->item(row, 1)->setText(names[row]);
        combo(components_, row, 3)
            ->setCurrentIndex(combo(components_, row, 3)->findData(positive[row]));
        combo(components_, row, 4)
            ->setCurrentIndex(combo(components_, row, 4)->findData(negative[row]));
        line(components_, row, 5)->setText(values[row]);
        if (row > 0)
            combo(components_, row, 6)->setCurrentIndex(1);
    }
    refresh_sources();
    reference_->setCurrentIndex(reference_->findData(0u));
    probe_positive_->setCurrentIndex(probe_positive_->findData(rlc ? 3u : 2u));
    probe_negative_->setCurrentIndex(probe_negative_->findData(0u));
    for (auto* unit : {frequency_unit_, start_unit_, stop_unit_}) {
        const QSignalBlocker block(unit);
        unit->setCurrentIndex(0);
        unit->setProperty("previousUnit", 0);
    }
    frequency_->setText(QString::number(rlc ? 1 / (2 * std::numbers::pi * std::sqrt(.01 * 1e-6))
                                            : 1 / (2 * std::numbers::pi * 1000 * 1e-6),
                                        'g', 17));
    start_->setText("10");
    stop_->setText(rlc ? "100000" : "10000");
    count_->setValue(201);
    mode_->setCurrentIndex(0);
    spacing_->setCurrentIndex(0);
    loading_ = false;
    solve();
    tabs_->setCurrentIndex(0);
}
ac::Circuit AcView::validated_circuit() const {
    using circuits::ComponentId;
    using circuits::NodeId;
    ac::CircuitDefinition draft;
    for (int row = 0; row < nodes_->rowCount(); ++row)
        draft.nodes.push_back({{nodes_->item(row, 0)->data(Qt::UserRole).toUInt()},
                               nodes_->item(row, 1)->text().toStdString()});
    if (ground_->currentData().isValid())
        draft.ground = NodeId{ground_->currentData().toUInt()};
    int voltage_count = 0;
    for (int row = 0; row < components_->rowCount(); ++row) {
        const auto name = components_->item(row, 1)->text().toStdString();
        const ComponentId id{components_->item(row, 0)->data(Qt::UserRole).toUInt()};
        const auto p = selected_node(combo(components_, row, 3)),
                   n = selected_node(combo(components_, row, 4));
        const double value = physical_value(line(components_, row, 5), combo(components_, row, 6));
        const int kind = combo(components_, row, 2)->currentIndex();
        ac::Phasor phasor;
        if (kind >= 3) {
            const auto degrees = circuit_value_si(line(components_, row, 7)->text(), 1);
            if (!degrees || value < 0)
                throw std::invalid_argument("Source RMS magnitude must be nonnegative and phase "
                                            "must be a finite number of degrees.");
            phasor = std::polar(value, std::remainder(*degrees, 360.) * std::numbers::pi / 180);
        }
        switch (kind) {
        case 0:
            draft.components.push_back(circuits::Resistor{id, name, p, n, value});
            break;
        case 1:
            draft.components.push_back(ac::Capacitor{id, name, p, n, value});
            break;
        case 2:
            draft.components.push_back(ac::Inductor{id, name, p, n, value});
            break;
        case 3:
            draft.components.push_back(ac::VoltageSource{id, name, p, n, phasor});
            ++voltage_count;
            break;
        case 4:
            draft.components.push_back(ac::CurrentSource{id, name, p, n, phasor});
            break;
        default:
            throw std::invalid_argument("Unknown AC component type.");
        }
    }
    if (voltage_count > ac_gui_limits::voltage_sources)
        throw std::invalid_argument("GUI voltage-source limit exceeded.");
    return ac::Circuit(std::move(draft));
}
void AcView::solve() {
    invalidate();
    try {
        const auto circuit = validated_circuit();
        const auto solution = ac::solve_ac(circuit, physical_value(frequency_, frequency_unit_));
        voltages_->setRowCount(static_cast<int>(solution.node_voltages.size()));
        for (int row = 0; row < voltages_->rowCount(); ++row)
            phasor_cells(voltages_, row,
                         nodes_->item(row, 1)->text() + " [" + nodes_->item(row, 0)->text() + "]",
                         solution.node_voltages[static_cast<std::size_t>(row)].voltage_volts_rms);
        currents_->setRowCount(static_cast<int>(solution.voltage_source_currents.size()));
        int row = 0;
        for (const auto& source : solution.voltage_source_currents) {
            QString name;
            for (const auto& part : circuit.definition().components)
                if (const auto* v = std::get_if<ac::VoltageSource>(&part);
                    v && v->id == source.source)
                    name = QString::fromStdString(v->name);
            phasor_cells(currents_, row++, name + " [" + QString::number(source.source.value) + "]",
                         source.current_amperes_rms);
        }
        status_->setText(QString("Solved AC at %1 Hz. RMS cosine reference; source currents + → −. "
                                 "Scaled reciprocal condition: %2; backward error: %3.")
                             .arg(number(solution.frequency_hz))
                             .arg(number(solution.quality.scaled_reciprocal_condition))
                             .arg(number(solution.quality.backward_error)));
        tabs_->setCurrentIndex(1);
    } catch (const circuits::CircuitError& e) {
        status_->setText(diagnostic(e));
    } catch (const std::exception& e) {
        status_->setText(QString::fromUtf8(e.what()));
    }
}
void AcView::start_sweep() {
    invalidate();
    try {
        auto circuit = validated_circuit();
        probe_ = {selected_node(probe_positive_), selected_node(probe_negative_)};
        for (auto id : {probe_.positive, probe_.negative})
            if (std::none_of(circuit.definition().nodes.begin(), circuit.definition().nodes.end(),
                             [&](const auto& n) { return n.id == id; }))
                throw circuits::CircuitError(circuits::ErrorCode::invalid_terminal,
                                             "Output probe references a missing node.", {id});
        normalized_ = mode_->currentIndex() == 0;
        if (normalized_) {
            if (!reference_->currentData().isValid())
                throw std::invalid_argument("Select a reference voltage source.");
            source_ = {reference_->currentData().toUInt()};
            ac::validate_voltage_transfer(circuit, {probe_, source_});
        }
        logarithmic_ = spacing_->currentIndex() == 0;
        frequencies_ = ac::frequency_grid(
            circuit,
            {physical_value(start_, start_unit_), physical_value(stop_, stop_unit_),
             static_cast<std::size_t>(count_->value()),
             logarithmic_ ? ac::FrequencySpacing::logarithmic : ac::FrequencySpacing::linear});
        run_circuit_.emplace(std::move(circuit));
        points_.reserve(frequencies_.size());
        magnitudes_.resize(frequencies_.size());
        phases_.resize(frequencies_.size());
        sweep_->setHorizontalHeaderLabels(
            normalized_ ? QStringList{"Frequency (Hz)", "Real H", "Imaginary H", "Gain (dB)",
                                      "Phase (°)", "Status"}
                        : QStringList{"Frequency (Hz)", "Real (V RMS)", "Imaginary (V RMS)",
                                      "Magnitude (V RMS)", "Phase (°)", "Status"});
        sweep_->setRowCount(static_cast<int>(frequencies_.size()));
        for (int row = 0; row < sweep_->rowCount(); ++row) {
            auto* item =
                fixed_item(QString::number(frequencies_[static_cast<std::size_t>(row)], 'g', 17));
            item->setData(Qt::UserRole, frequencies_[static_cast<std::size_t>(row)]);
            sweep_->setItem(row, 0, item);
            for (int col = 1; col < 5; ++col)
                sweep_->setItem(row, col, fixed_item(QString::fromUtf8("—")));
            sweep_->setItem(row, 5, fixed_item("Pending"));
        }
        update_plots();
        tabs_->setCurrentIndex(2);
        cancel_->setEnabled(true);
        status_->setText("Sweep running: 0 frequencies evaluated.");
        timer_->start();
    } catch (const circuits::CircuitError& e) {
        invalidate();
        status_->setText(diagnostic(e));
    } catch (const std::exception& e) {
        invalidate();
        status_->setText(QString::fromUtf8(e.what()));
    }
}
void AcView::advance_sweep() {
    if (!run_circuit_ || points_.size() >= frequencies_.size())
        return;
    const auto index = points_.size();
    const int row = static_cast<int>(index);
    try {
        points_.push_back(ac::solve_ac_point(*run_circuit_, frequencies_[index]));
        const auto& point = points_.back();
        if (const auto* error = std::get_if<circuits::CircuitError>(&point.result)) {
            ++failures_;
            sweep_->item(row, 5)->setText(diagnostic(*error));
            sweep_->item(row, 5)->setData(Qt::UserRole, static_cast<int>(error->code()));
        } else {
            const auto& solution = std::get<ac::AcSolution>(point.result);
            try {
                const auto value =
                    normalized_ ? ac::voltage_transfer(*run_circuit_, solution, {probe_, source_})
                                : ac::probe_voltage(solution, probe_);
                const double magnitude =
                    normalized_ ? ac::gain_magnitude_db(value) : std::abs(value);
                const auto phase = (normalized_ && magnitude <= ac::gain_display_floor_db)
                                       ? std::optional<double>{}
                                       : ac::wrapped_phase_degrees(value);
                magnitudes_[index] = magnitude;
                phases_[index] = phase;
                sweep_->item(row, 1)->setText(number(value.real()));
                sweep_->item(row, 2)->setText(number(value.imag()));
                sweep_->item(row, 3)->setText(number(magnitude));
                sweep_->item(row, 4)->setText(phase ? number(*phase) : QString::fromUtf8("—"));
                sweep_->item(row, 5)->setText("Accepted");
            } catch (const circuits::CircuitError& e) {
                ++failures_;
                sweep_->item(row, 5)->setText("Response display: " + diagnostic(e));
            }
        }
        const bool done = points_.size() == frequencies_.size();
        status_->setText(QString(done ? "Sweep complete: %1/%2 evaluated; %3 failures."
                                      : "Sweep running: %1/%2 evaluated; %3 failures.")
                             .arg(points_.size())
                             .arg(frequencies_.size())
                             .arg(failures_));
        if (done || points_.size() % 16 == 0)
            update_plots();
        if (done) {
            timer_->stop();
            cancel_->setEnabled(false);
        }
    } catch (const std::exception& e) {
        cancel_sweep();
        status_->setText("Sweep interrupted: " + QString::fromUtf8(e.what()));
    }
}
void AcView::cancel_sweep() {
    timer_->stop();
    cancel_->setEnabled(false);
    if (!run_circuit_)
        return;
    for (std::size_t i = points_.size(); i < frequencies_.size(); ++i)
        sweep_->item(static_cast<int>(i), 5)->setText("Not evaluated (cancelled)");
    update_plots();
    status_->setText(
        QString("Sweep cancelled: %1/%2 evaluated; remaining frequencies were not solved.")
            .arg(points_.size())
            .arg(frequencies_.size()));
}
void AcView::update_plots() {
    magnitude_plot_->set_response(frequencies_, magnitudes_, logarithmic_,
                                  normalized_ ? "Voltage gain (dB, floor −240)"
                                              : "Magnitude (V RMS)");
    phase_plot_->set_response(frequencies_, phases_, logarithmic_, "Phase (degrees)");
}
} // namespace openece::gui
