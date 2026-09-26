#pragma once
#include "circuit_value.hpp"
#include "draft_view.hpp"
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
namespace openece::gui {
enum class CircuitDraftChange { value, nodes, components };
inline project::Reference selected_reference(QComboBox* box) {
    return box->currentData().isValid()
               ? project::Reference{project::Id{box->currentData().toUInt()}}
               : std::nullopt;
}
inline void select_reference(QComboBox* box, project::Reference ref) {
    const QSignalBlocker block(box);
    int i = 0;
    if (ref) {
        i = box->findData(ref->value);
        if (i < 0) {
            box->addItem("Missing node " + QString::number(ref->value), ref->value);
            i = box->count() - 1;
        }
    }
    box->setCurrentIndex(i);
}
inline QStringList component_units(const std::string& kind, bool ac) {
    if (kind == "resistor")
        return ac ? QStringList{"ohm", "kohm", "Mohm"} : QStringList{"ohm", "kohm", "Mohm", "mohm"};
    if (kind == "capacitor")
        return {"F", "uF", "nF", "pF"};
    if (kind == "inductor")
        return {"H", "mH", "uH"};
    if (kind == "voltage_source")
        return {"V", "mV", "uV"};
    return {"A", "mA", "uA"};
}
inline double unit_factor(const QString& token) {
    if (token == "kohm")
        return 1e3;
    if (token == "Mohm")
        return 1e6;
    if (token.startsWith('m'))
        return 1e-3;
    if (token.startsWith('u'))
        return 1e-6;
    if (token.startsWith('n'))
        return 1e-9;
    if (token.startsWith('p'))
        return 1e-12;
    return 1;
}
// The rows are views of the domain draft, with stable IDs captured by control callbacks.
// No references into resizable vectors or independent editable circuit definition are kept.
template <class Draft> class CircuitDraftRows {
    static constexpr bool ac = std::is_same_v<Draft, project::AcDraft>;
    Draft& draft_;
    QTableWidget *nodes_, *parts_;
    QComboBox* ground_;
    QLabel* status_;
    QObject* context_;
    std::function<void(CircuitDraftChange)> changed_;
    void notify(CircuitDraftChange what = CircuitDraftChange::value) { changed_(what); }
    bool rendering_ = false;
    auto& part(project::Id id) {
        return *std::ranges::find(
            draft_.components, id,
            &std::remove_reference_t<decltype(draft_.components.front())>::id);
    }
    static QStringList kinds() {
        return ac ? QStringList{"resistor", "capacitor", "inductor", "voltage_source",
                                "current_source"}
                  : QStringList{"resistor", "voltage_source", "current_source"};
    }
    static QTableWidgetItem* fixed(project::Id id) {
        auto* i = new QTableWidgetItem(QString::number(id.value));
        i->setFlags(i->flags() & ~Qt::ItemIsEditable);
        i->setData(Qt::UserRole, id.value);
        return i;
    }
    void fill_units(QComboBox* box, const std::string& kind, const std::string& selected) {
        const QSignalBlocker block(box);
        box->clear();
        for (auto token : component_units(kind, ac)) {
            auto display = token;
            display.replace("ohm", QString::fromUtf8("Ω"));
            if (display.startsWith('u'))
                display.replace(0, 1, QString::fromUtf8("µ"));
            box->addItem(display, unit_factor(token));
        }
        box->setCurrentIndex(
            static_cast<int>(component_units(kind, ac).indexOf(qt_text(selected))));
        box->setProperty("previousUnit", box->currentIndex());
    }
    void append_node(const project::Node& n) {
        int row = nodes_->rowCount();
        nodes_->insertRow(row);
        nodes_->setItem(row, 0, fixed(n.id));
        nodes_->setItem(row, 1, new QTableWidgetItem(qt_text(n.name)));
    }
    void append_part(std::size_t index) {
        const auto& initial = draft_.components[index];
        const auto id = initial.id;
        int row = parts_->rowCount();
        parts_->insertRow(row);
        parts_->setItem(row, 0, fixed(id));
        parts_->setItem(row, 1, new QTableWidgetItem(qt_text(initial.name)));
        auto* type = new QComboBox(parts_);
        type->addItems(ac ? QStringList{"Resistor", "Capacitor", "Inductor", "AC voltage source",
                                        "AC current source"}
                          : QStringList{"Resistor", "Voltage source", "Current source"});
        type->setCurrentIndex(static_cast<int>(kinds().indexOf(qt_text(initial.kind))));
        parts_->setCellWidget(row, 2, type);
        for (int col : {3, 4}) {
            auto* selector = new QComboBox(parts_);
            parts_->setCellWidget(row, col, selector);
            fill_nodes(selector, col == 3 ? initial.positive : initial.negative);
            QObject::connect(selector, &QComboBox::currentIndexChanged, context_,
                             [this, id, col, selector] {
                                 if (rendering_)
                                     return;
                                 auto& ref = col == 3 ? part(id).positive : part(id).negative;
                                 auto value = selected_reference(selector);
                                 if (ref != value) {
                                     ref = value;
                                     notify();
                                 }
                             });
        }
        auto* value = new QLineEdit(qt_text(initial.value.text), parts_);
        value->setMaxLength(128);
        value->setMinimumWidth(85);
        parts_->setCellWidget(row, 5, value);
        auto* unit = new QComboBox(parts_);
        fill_units(unit, initial.kind, initial.value.unit);
        parts_->setCellWidget(row, 6, unit);
        QLineEdit* phase = nullptr;
        if constexpr (ac) {
            phase = new QLineEdit(qt_text(initial.phase.text), parts_);
            phase->setMaxLength(128);
            phase->setMaximumWidth(90);
            phase->setEnabled(type->currentIndex() >= 3);
            parts_->setCellWidget(row, 7, phase);
            QObject::connect(phase, &QLineEdit::textChanged, context_, [this, id, phase] {
                if (rendering_)
                    return;
                auto s = draft_text(phase->text());
                if (part(id).phase.text != s) {
                    part(id).phase.text = s;
                    notify();
                }
            });
        }
        QObject::connect(value, &QLineEdit::textChanged, context_, [this, id, value] {
            if (rendering_)
                return;
            auto s = draft_text(value->text());
            if (part(id).value.text != s) {
                part(id).value.text = s;
                notify();
            }
        });
        QObject::connect(type, &QComboBox::currentIndexChanged, context_,
                         [this, id, type, unit, value, phase] {
                             // The DC specialization has no phase control.
                             static_cast<void>(phase);
                             if (rendering_)
                                 return;
                             auto& p = part(id);
                             p.kind = draft_text(kinds()[type->currentIndex()]);
                             p.value = {"", draft_text(component_units(p.kind, ac)[0])};
                             fill_units(unit, p.kind, p.value.unit);
                             value->clear();
                             if constexpr (ac) {
                                 p.phase = {"0", "deg"};
                                 phase->setText("0");
                                 phase->setEnabled(type->currentIndex() >= 3);
                             }
                             notify(CircuitDraftChange::components);
                         });
        QObject::connect(unit, &QComboBox::currentIndexChanged, context_, [this, id, unit, value] {
            if (rendering_)
                return;
            auto& p = part(id);
            auto tokens = component_units(p.kind, ac);
            const int selected = unit->currentIndex(),
                      previous = static_cast<int>(tokens.indexOf(qt_text(p.value.unit)));
            if (selected == previous)
                return;
            auto physical = circuit_value_si(value->text(), unit->itemData(previous).toDouble());
            double converted = physical ? *physical / unit->itemData(selected).toDouble() : 0;
            if (!physical || !std::isfinite(converted) || (*physical != 0 && converted == 0)) {
                QSignalBlocker block(unit);
                unit->setCurrentIndex(previous);
                status_->setText("Invalid pending value. Correct it before changing units.");
                return;
            }
            p.value.unit = draft_text(tokens[selected]);
            unit->setProperty("previousUnit", selected);
            value->setText(QString::number(converted, 'g', 17));
            notify();
        });
    }

  public:
    CircuitDraftRows(Draft& draft, QTableWidget* nodes, QTableWidget* parts, QComboBox* ground,
                     QLabel* status, QObject* context,
                     std::function<void(CircuitDraftChange)> changed)
        : draft_(draft), nodes_(nodes), parts_(parts), ground_(ground), status_(status),
          context_(context), changed_(std::move(changed)) {
        QObject::connect(nodes_, &QTableWidget::itemChanged, context_, [this](QTableWidgetItem* i) {
            text_edit(nodes_, i->row(), i->column(), i->text());
        });
        QObject::connect(parts_, &QTableWidget::itemChanged, context_, [this](QTableWidgetItem* i) {
            text_edit(parts_, i->row(), i->column(), i->text());
        });
        QObject::connect(ground_, &QComboBox::currentIndexChanged, context_, [this] {
            if (!rendering_) {
                auto r = selected_reference(ground_);
                if (draft_.ground != r) {
                    draft_.ground = r;
                    notify();
                }
            }
        });
    }
    void fill_nodes(QComboBox* box, project::Reference ref) {
        const QSignalBlocker block(box);
        box->clear();
        box->addItem("Select node");
        for (const auto& n : draft_.nodes)
            box->addItem(qt_text(n.name) + " [" + QString::number(n.id.value) + "]", n.id.value);
        select_reference(box, ref);
    }
    void refresh() {
        fill_nodes(ground_, draft_.ground);
        for (int row = 0; row < parts_->rowCount(); ++row) {
            auto& p = draft_.components[static_cast<std::size_t>(row)];
            fill_nodes(static_cast<QComboBox*>(parts_->cellWidget(row, 3)), p.positive);
            fill_nodes(static_cast<QComboBox*>(parts_->cellWidget(row, 4)), p.negative);
        }
    }
    void render() {
        rendering_ = true;
        const QSignalBlocker a(nodes_), b(parts_);
        parts_->setRowCount(0);
        nodes_->setRowCount(0);
        for (const auto& n : draft_.nodes)
            append_node(n);
        for (std::size_t i = 0; i < draft_.components.size(); ++i)
            append_part(i);
        refresh();
        rendering_ = false;
    }
    void text_edit(QTableWidget* table, int row, int col, const QString& text) {
        if (rendering_ || col != 1 || row < 0)
            return;
        auto& target = table == nodes_ ? draft_.nodes.at(static_cast<std::size_t>(row)).name
                                       : draft_.components.at(static_cast<std::size_t>(row)).name;
        auto value = draft_text(text);
        if (target != value) {
            target = value;
            if (table == nodes_) {
                refresh();
                notify(CircuitDraftChange::nodes);
            } else
                notify(CircuitDraftChange::components);
        }
    }
    void add_node(const QString& name) {
        if (draft_.nodes.size() >= project::limits::circuit_nodes) {
            status_->setText("Node limit reached.");
            return;
        }
        try {
            auto id = project::allocate_id(draft_.next_node, project::reserved_node_ids(draft_));
            draft_.nodes.push_back(
                {id, name.isEmpty() ? "N" + std::to_string(id.value) : draft_text(name)});
            rendering_ = true;
            {
                const QSignalBlocker block(nodes_);
                append_node(draft_.nodes.back());
            }
            refresh();
            rendering_ = false;
            notify(CircuitDraftChange::nodes);
        } catch (const std::exception& e) {
            status_->setText(QString::fromUtf8(e.what()));
        }
    }
    void add_component() {
        if (draft_.components.size() >= project::limits::circuit_components) {
            status_->setText("Component limit reached.");
            return;
        }
        try {
            auto id = project::allocate_id(draft_.next_component,
                                           project::reserved_component_ids(draft_));
            typename std::remove_reference_t<decltype(draft_.components)>::value_type p;
            p.id = id;
            p.name = (ac ? "P" : "C") + std::to_string(id.value);
            draft_.components.push_back(p);
            rendering_ = true;
            {
                const QSignalBlocker block(parts_);
                append_part(draft_.components.size() - 1);
            }
            rendering_ = false;
            notify(CircuitDraftChange::components);
        } catch (const std::exception& e) {
            status_->setText(QString::fromUtf8(e.what()));
        }
    }
    void remove_node(int row) {
        if (row < 0)
            return;
        draft_.nodes.erase(draft_.nodes.begin() + row);
        nodes_->removeRow(row);
        refresh();
        notify(CircuitDraftChange::nodes);
    }
    void remove_component(int row) {
        if (row < 0)
            return;
        draft_.components.erase(draft_.components.begin() + row);
        parts_->removeRow(row);
        notify(CircuitDraftChange::components);
    }
};
} // namespace openece::gui
