#include "circuits_view.hpp"
#include "circuit_value.hpp"
#include <QComboBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>
#include <openece/circuits/dc.hpp>
namespace openece::gui {
namespace {
QTableWidget* table(const char* name, const QStringList& columns, QWidget* parent) {
    auto* t = new QTableWidget(0, static_cast<int>(columns.size()), parent);
    t->setObjectName(name);
    t->setHorizontalHeaderLabels(columns);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->verticalHeader()->hide();
    return t;
}
QTableWidgetItem* fixed_item(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}
QComboBox* combo(QTableWidget* table, int row, int column) {
    return static_cast<QComboBox*>(table->cellWidget(row, column));
}

} // namespace
CircuitsView::CircuitsView(QWidget* parent, project::DcDraft* draft, bool inert)
    : DraftView(parent), state_(draft, project::default_project().circuits.dc) {
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel("Linear DC circuit analysis", this);
    auto font = title->font();
    font.setPointSize(font.pointSize() + 3);
    title->setFont(font);
    layout->addWidget(title);
    auto* tabs = new QTabWidget(this);
    tabs_ = tabs;
    tabs->setObjectName("dc_view_tabs");
    layout->addWidget(tabs);
    auto* editor = new QWidget(tabs);
    tabs->addTab(editor, "Circuit and results");
    auto* body = new QVBoxLayout(editor);
    auto* editors = new QHBoxLayout;
    body->addLayout(editors, 2);
    auto* node_group = new QGroupBox("Nodes and reference", editor);
    auto* nl = new QVBoxLayout(node_group);
    nodes_ = table("circuit_nodes", {"ID", "Name"}, node_group);
    nl->addWidget(nodes_);
    ground_ = new QComboBox(node_group);
    ground_->setObjectName("circuit_ground");
    ground_->setAccessibleName("Ground node");
    nl->addWidget(new QLabel("Ground (0 V)", node_group));
    nl->addWidget(ground_);
    auto* nb = new QHBoxLayout;
    nl->addLayout(nb);
    auto* add_n = new QPushButton("Add node", node_group);
    add_n->setObjectName("circuit_add_node");
    nb->addWidget(add_n);
    auto* remove_n = new QPushButton("Remove node", node_group);
    remove_n->setObjectName("circuit_remove_node");
    nb->addWidget(remove_n);
    editors->addWidget(node_group, 1);
    auto* parts_group = new QGroupBox("Components — edit cells in place", editor);
    auto* pl = new QVBoxLayout(parts_group);
    components_ =
        table("circuit_components",
              {"ID", "Name", "Type", "Positive (+)", "Negative (−)", "Value", "Unit"}, parts_group);
    pl->addWidget(components_);
    auto* pb = new QHBoxLayout;
    pl->addLayout(pb);
    auto* add_c = new QPushButton("Add component", parts_group);
    add_c->setObjectName("circuit_add_component");
    pb->addWidget(add_c);
    auto* remove_c = new QPushButton("Remove component", parts_group);
    remove_c->setObjectName("circuit_remove_component");
    pb->addWidget(remove_c);
    editors->addWidget(parts_group, 3);
    auto* actions = new QHBoxLayout;
    body->addLayout(actions);
    auto* solve_button = new QPushButton("Solve DC", editor);
    solve_button->setObjectName("circuit_solve");
    actions->addWidget(solve_button);
    auto* example = new QPushButton("Load voltage divider", editor);
    example->setObjectName("circuit_example");
    actions->addWidget(example);
    actions->addStretch();
    status_ = new QLabel(editor);
    status_->setObjectName("circuit_status");
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->addWidget(status_);
    auto* results = new QHBoxLayout;
    body->addLayout(results, 1);
    voltages_ = table("circuit_voltages", {"Node", "Voltage (V)"}, editor);
    currents_ = table("circuit_currents", {"Voltage source", "Current + → − (A)"}, editor);
    voltages_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    currents_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results->addWidget(voltages_);
    results->addWidget(currents_);
    auto* help = new QTextBrowser(tabs);
    help->setHtml(QString::fromUtf8(
        "<h2>DC conventions</h2><p>Resistors and independent DC sources only. Select an explicit "
        "ground. "
        "Connections use stable IDs; names are labels. Removing a node leaves its references "
        "missing until you fix them.</p>"
        "<p>For every component, positive current flows from + to −. A voltage source imposes V(+) "
        "− V(−) = value. "
        "A 10 V source supplying a 1 kΩ resistor therefore has current <b>−0.01 A</b>.</p>"
        "<p>Values use decimal/scientific notation and a dot decimal separator. Units convert the "
        "current physical value, "
        "including pending edits. Invalid text remains visible. Changing component type clears the "
        "old value.</p>"
        "<p>Resistance: 1e−9 to 1e12 Ω. Source magnitude: zero or 1e−12 to 1e9 V/A. "
        "Negative source values are allowed. Zero-ohm resistors are rejected.</p>"
        "<p>Current sources do not establish a voltage reference. Floating networks and redundant "
        "or contradictory "
        "ideal-voltage-source loops are rejected. No hidden grounding or regularization is "
        "performed.</p>"
        "<p>Results are cleared on editing. Numerical policy failures mean no accepted solution; "
        "solver thresholds "
        "are not physical component tolerances. GUI limits: 32 nodes, 128 components, 32 voltage "
        "sources.</p>"));
    tabs->addTab(help, "Conventions and help");
    connect(add_n, &QPushButton::clicked, this, [this] { add_node(); });
    connect(add_c, &QPushButton::clicked, this, [this] { add_component(); });
    connect(remove_n, &QPushButton::clicked, this, [this] {
        if (nodes_->currentRow() >= 0) {
            rows_->remove_node(nodes_->currentRow());
            refresh_connections();
            invalidate();
        }
    });
    connect(remove_c, &QPushButton::clicked, this, [this] {
        if (components_->currentRow() >= 0) {
            rows_->remove_component(components_->currentRow());
            invalidate();
        }
    });
    connect(nodes_, &QTableWidget::itemChanged, this, [this] {
        if (!loading_) {
            refresh_connections();
            invalidate();
        }
    });
    connect(components_, &QTableWidget::itemChanged, this, [this] { invalidate(); });
    connect(ground_, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });
    connect(solve_button, &QPushButton::clicked, this, [this] { solve(); });
    connect(example, &QPushButton::clicked, this, [this] { load_divider(); });
    rows_ = std::make_unique<CircuitDraftRows<project::DcDraft>>(state_.get(), nodes_, components_,
                                                                 ground_, status_, this, [this] {
                                                                     edited();
                                                                     invalidate();
                                                                 });
    bind_table(nodes_,
               [this](int r, int c, const QString& t) { rows_->text_edit(nodes_, r, c, t); });
    bind_table(components_,
               [this](int r, int c, const QString& t) { rows_->text_edit(components_, r, c, t); });
    rows_->render();
    bind_tabs(tabs_, state_.get().selected_tab, {"editor", "help"});
    loading_ = false;
    restoring_ = false;
    if (!inert) {
        solve();
    } else {
        invalidate();
        status_->clear();
    }
}
void CircuitsView::invalidate() {
    if (loading_)
        return;
    voltages_->setRowCount(0);
    currents_->setRowCount(0);
    status_->setText("Draft changed. Solve to validate and calculate results.");
}
void CircuitsView::add_node(const QString& name) { rows_->add_node(name); }
void CircuitsView::add_component() { rows_->add_component(); }
void CircuitsView::refresh_connection(QComboBox* selector) {
    if (rows_)
        rows_->fill_nodes(selector, selected_reference(selector));
}
void CircuitsView::refresh_connections() {
    refresh_connection(ground_);
    for (int i = 0; i < components_->rowCount(); ++i)
        for (int column : {3, 4})
            refresh_connection(combo(components_, i, column));
}
void CircuitsView::load_divider() {
    const auto selected_tab = state_.get().selected_tab;
    state_.get() = project::default_project().circuits.dc;
    state_.get().selected_tab = selected_tab;
    loading_ = true;
    rows_->render();
    loading_ = false;
    edited();
    solve();
}
void CircuitsView::solve() {
    synchronize_pending_text();
    invalidate();
    try {
        using namespace circuits;
        CircuitDefinition draft;
        const auto& model = state_.get();
        for (const auto& node : model.nodes)
            draft.nodes.push_back({{node.id.value}, node.name});
        if (model.ground)
            draft.ground = NodeId{model.ground->value};
        int voltage_count = 0;
        for (const auto& part : model.components) {
            const auto& name = part.name;
            ComponentId id{part.id.value};
            if (!part.positive || !part.negative)
                throw std::invalid_argument("Select both terminals for component " + name);
            NodeId p{part.positive->value}, n{part.negative->value};
            auto value =
                circuit_value_si(qt_text(part.value.text), unit_factor(qt_text(part.value.unit)));
            if (!value)
                throw std::invalid_argument("Invalid numeric value for component " + name);
            if (part.kind == "resistor")
                draft.components.push_back(Resistor{id, name, p, n, *value});
            else if (part.kind == "voltage_source") {
                draft.components.push_back(VoltageSource{id, name, p, n, *value});
                ++voltage_count;
            } else
                draft.components.push_back(CurrentSource{id, name, p, n, *value});
        }
        if (voltage_count > circuit_gui_limits::voltage_sources)
            throw std::invalid_argument("GUI voltage-source limit exceeded.");
        const Circuit circuit(std::move(draft));
        const auto solution = solve_dc(circuit);
        voltages_->setRowCount(static_cast<int>(solution.node_voltages.size()));
        for (int i = 0; i < voltages_->rowCount(); ++i) {
            voltages_->setItem(
                i, 0,
                fixed_item(qt_text(model.nodes[static_cast<std::size_t>(i)].name) + " [" +
                           QString::number(model.nodes[static_cast<std::size_t>(i)].id.value) +
                           "]"));
            voltages_->setItem(
                i, 1,
                fixed_item(QString::number(
                    solution.node_voltages[static_cast<std::size_t>(i)].voltage_volts, 'g', 12)));
        }
        currents_->setRowCount(static_cast<int>(solution.voltage_source_currents.size()));
        int row = 0;
        for (const auto& source : solution.voltage_source_currents) {
            auto it =
                std::find_if(circuit.definition().components.begin(),
                             circuit.definition().components.end(), [&](const auto& c) {
                                 return std::visit(
                                     [&](const auto& part) { return part.id == source.source; }, c);
                             });
            currents_->setItem(
                row, 0,
                fixed_item(QString::fromStdString(std::get<VoltageSource>(*it).name) + " [" +
                           QString::number(source.source.value) + "]"));
            currents_->setItem(row++, 1,
                               fixed_item(QString::number(source.current_amperes, 'g', 12)));
        }
        status_->setText(QString("Solved DC. Scaled reciprocal condition: %1; backward error: %2. "
                                 "Currents use + → −.")
                             .arg(solution.quality.scaled_reciprocal_condition, 0, 'g', 3)
                             .arg(solution.quality.backward_error, 0, 'g', 3));
    } catch (const circuits::CircuitError& e) {
        QString message = QString::fromUtf8(e.what());
        for (auto id : e.nodes())
            message += " Node ID " + QString::number(id.value) + ".";
        for (auto id : e.components())
            message += " Component ID " + QString::number(id.value) + ".";
        status_->setText(message);
    } catch (const std::exception& e) {
        status_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace openece::gui
