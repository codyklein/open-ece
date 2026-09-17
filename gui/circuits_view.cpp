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
void units(QComboBox* selector, int type) {
    QSignalBlocker block(selector);
    selector->clear();
    if (type == 0) {
        selector->addItem(QString::fromUtf8("Ω"), 1.0);
        selector->addItem(QString::fromUtf8("kΩ"), 1e3);
        selector->addItem(QString::fromUtf8("MΩ"), 1e6);
        selector->addItem(QString::fromUtf8("mΩ"), 1e-3);
    } else {
        const QString base = type == 1 ? "V" : "A";
        selector->addItem(base, 1.0);
        selector->addItem("m" + base, 1e-3);
        selector->addItem(QString::fromUtf8("µ") + base, 1e-6);
    }
    selector->setProperty("previousUnit", 0);
}
} // namespace
CircuitsView::CircuitsView(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel("Linear DC circuit analysis", this);
    auto font = title->font();
    font.setPointSize(font.pointSize() + 3);
    title->setFont(font);
    layout->addWidget(title);
    auto* tabs = new QTabWidget(this);
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
            nodes_->removeRow(nodes_->currentRow());
            refresh_connections();
            invalidate();
        }
    });
    connect(remove_c, &QPushButton::clicked, this, [this] {
        if (components_->currentRow() >= 0) {
            components_->removeRow(components_->currentRow());
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
    load_divider();
}
void CircuitsView::invalidate() {
    if (loading_)
        return;
    voltages_->setRowCount(0);
    currents_->setRowCount(0);
    status_->setText("Draft changed. Solve to validate and calculate results.");
}
void CircuitsView::add_node(const QString& name) {
    if (nodes_->rowCount() >= circuit_gui_limits::nodes ||
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
void CircuitsView::add_component() {
    if (components_->rowCount() >= circuit_gui_limits::components ||
        next_component_ > std::numeric_limits<std::uint32_t>::max()) {
        status_->setText("Component limit reached.");
        return;
    }
    const QSignalBlocker block(components_);
    int row = components_->rowCount();
    components_->insertRow(row);
    auto id = static_cast<unsigned>(next_component_++);
    auto* item = fixed_item(QString::number(id));
    item->setData(Qt::UserRole, id);
    components_->setItem(row, 0, item);
    components_->setItem(row, 1, new QTableWidgetItem("C" + QString::number(id)));
    auto* type = new QComboBox(components_);
    type->addItems({"Resistor", "Voltage source", "Current source"});
    components_->setCellWidget(row, 2, type);
    for (int column : {3, 4}) {
        auto* selector = new QComboBox(components_);
        components_->setCellWidget(row, column, selector);
        connect(selector, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });
    }
    auto* value = new QLineEdit(components_);
    value->setMaxLength(128);
    value->setMinimumWidth(85);
    value->setPlaceholderText("SI value");
    components_->setCellWidget(row, 5, value);
    auto* unit = new QComboBox(components_);
    units(unit, 0);
    components_->setCellWidget(row, 6, unit);
    connect(type, &QComboBox::currentIndexChanged, this, [this, unit, value](int kind) {
        units(unit, kind);
        value->clear();
        invalidate();
    });
    connect(value, &QLineEdit::textChanged, this, [this] { invalidate(); });
    connect(unit, &QComboBox::currentIndexChanged, this, [this, unit, value](int selected) {
        int previous = unit->property("previousUnit").toInt();
        if (selected == previous)
            return;
        auto si = circuit_value_si(value->text(), unit->itemData(previous).toDouble());
        double converted = si ? *si / unit->itemData(selected).toDouble() : 0;
        if (!si || !std::isfinite(converted) || (*si != 0 && converted == 0)) {
            QSignalBlocker block_unit(unit);
            unit->setCurrentIndex(previous);
            invalidate();
            status_->setText("Invalid pending value. Correct it before changing units.");
            return;
        }
        unit->setProperty("previousUnit", selected);
        value->setText(QString::number(converted, 'g', 17));
        invalidate();
    });
    refresh_connections();
    invalidate();
}
void CircuitsView::refresh_connections() {
    auto refresh = [&](QComboBox* selector) {
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
    };
    refresh(ground_);
    for (int i = 0; i < components_->rowCount(); ++i)
        for (int column : {3, 4})
            refresh(combo(components_, i, column));
}
void CircuitsView::load_divider() {
    loading_ = true;
    components_->setRowCount(0);
    nodes_->setRowCount(0);
    next_node_ = next_component_ = 0;
    ground_->clear();
    add_node("Ground");
    add_node("Supply");
    add_node("Midpoint");
    ground_->setCurrentIndex(ground_->findData(0u));
    for (int i = 0; i < 3; ++i)
        add_component();
    combo(components_, 0, 2)->setCurrentIndex(1);
    const unsigned positives[] = {1, 1, 2}, negatives[] = {0, 2, 0};
    for (int i = 0; i < 3; ++i) {
        components_->item(i, 1)->setText(i == 0 ? "V1" : "R" + QString::number(i));
        combo(components_, i, 3)->setCurrentIndex(combo(components_, i, 3)->findData(positives[i]));
        combo(components_, i, 4)->setCurrentIndex(combo(components_, i, 4)->findData(negatives[i]));
        static_cast<QLineEdit*>(components_->cellWidget(i, 5))->setText(i == 0 ? "10" : "1000");
    }
    loading_ = false;
    invalidate();
    solve();
}
void CircuitsView::solve() {
    invalidate();
    try {
        using namespace circuits;
        CircuitDefinition draft;
        for (int i = 0; i < nodes_->rowCount(); ++i)
            draft.nodes.push_back({{nodes_->item(i, 0)->data(Qt::UserRole).toUInt()},
                                   nodes_->item(i, 1)->text().toStdString()});
        if (ground_->currentData().isValid())
            draft.ground = NodeId{ground_->currentData().toUInt()};
        int voltage_count = 0;
        for (int row = 0; row < components_->rowCount(); ++row) {
            auto name = components_->item(row, 1)->text();
            ComponentId id{components_->item(row, 0)->data(Qt::UserRole).toUInt()};
            if (!combo(components_, row, 3)->currentData().isValid() ||
                !combo(components_, row, 4)->currentData().isValid())
                throw std::invalid_argument("Select both terminals for component " +
                                            name.toStdString());
            NodeId p{combo(components_, row, 3)->currentData().toUInt()},
                n{combo(components_, row, 4)->currentData().toUInt()};
            auto value =
                circuit_value_si(static_cast<QLineEdit*>(components_->cellWidget(row, 5))->text(),
                                 combo(components_, row, 6)->currentData().toDouble());
            if (!value)
                throw std::invalid_argument("Invalid numeric value for component " +
                                            name.toStdString());
            switch (combo(components_, row, 2)->currentIndex()) {
            case 0:
                draft.components.push_back(Resistor{id, name.toStdString(), p, n, *value});
                break;
            case 1:
                draft.components.push_back(VoltageSource{id, name.toStdString(), p, n, *value});
                ++voltage_count;
                break;
            default:
                draft.components.push_back(CurrentSource{id, name.toStdString(), p, n, *value});
                break;
            }
        }
        if (voltage_count > circuit_gui_limits::voltage_sources)
            throw std::invalid_argument("GUI voltage-source limit exceeded.");
        const Circuit circuit(std::move(draft));
        const auto solution = solve_dc(circuit);
        voltages_->setRowCount(static_cast<int>(solution.node_voltages.size()));
        for (int i = 0; i < voltages_->rowCount(); ++i) {
            voltages_->setItem(
                i, 0,
                fixed_item(nodes_->item(i, 1)->text() + " [" + nodes_->item(i, 0)->text() + "]"));
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
