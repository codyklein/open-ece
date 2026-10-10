#include "transient_view.hpp"
#include "circuit_draft_rows.hpp"
#include "transient_results.hpp"
#include <QEvent>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSplitter>
#include <QStyle>
#include <QTableView>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
namespace openece::gui {
namespace {
const QStringList kinds{"resistor", "capacitor", "inductor", "voltage_source", "current_source"};
const QStringList kind_labels{"Resistor", "Capacitor", "Inductor", "Voltage Source",
                              "Current Source"};
const QStringList initial_kinds{"capacitor_voltage", "inductor_current"};
const QStringList initial_labels{"Capacitor voltage", "Inductor current"};
const QStringList times{"s", "ms", "us", "ns"};
class TransientEditorScroll final : public QScrollArea {
  public:
    using QScrollArea::QScrollArea;
    QSize sizeHint() const override {
        // Preserve the compact editor's natural initial height. QScrollArea's
        // capped/cached hint can otherwise hide even the third component row.
        return widget() ? widget()->sizeHint() + QSize(2 * frameWidth(), 2 * frameWidth())
                        : QScrollArea::sizeHint();
    }
};
class TransientSplitterHandle final : public QSplitterHandle {
  public:
    explicit TransientSplitterHandle(QSplitter* parent) : QSplitterHandle(Qt::Vertical, parent) {
        setAccessibleName("Resize transient editor and results");
        setToolTip("Drag to resize the editor and results; both panels remain visible.");
    }

  protected:
    void paintEvent(QPaintEvent* event) override {
        QSplitterHandle::paintEvent(event);
        // Use the active theme's foreground rather than a fixed light/dark color.
        QPainter painter(this);
        auto color = palette().color(QPalette::WindowText);
        color.setAlpha(180);
        painter.setPen(QPen(color, 2));
        const auto center = rect().center();
        const int half_width = std::min(width() / 4, fontMetrics().height());
        for (int offset : {-2, 2})
            painter.drawLine(center.x() - half_width, center.y() + offset, center.x() + half_width,
                             center.y() + offset);
    }
};
class TransientSplitter final : public QSplitter {
  public:
    explicit TransientSplitter(QWidget* parent) : QSplitter(Qt::Vertical, parent) {
        setOpaqueResize(true);
        setChildrenCollapsible(false);
        fit_handle();
    }
    QSize sizeHint() const override {
        auto size = QSplitter::sizeHint();
        if (count() == 2)
            size.setHeight(widget(0)->sizeHint().height() + widget(1)->sizeHint().height() +
                           handleWidth());
        return size;
    }

  protected:
    QSplitterHandle* createHandle() override { return new TransientSplitterHandle(this); }
    void changeEvent(QEvent* event) override {
        QSplitter::changeEvent(event);
        if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
            fit_handle();
    }

  private:
    void fit_handle() {
        setHandleWidth(std::max(12, fontMetrics().height()));
        if (count() > 0)
            widget(0)->setMinimumHeight(6 * fontMetrics().height());
    }
};
QStringList units(const std::string& kind) {
    if (kind == "capacitor")
        return {"F", "mF", "uF", "nF", "pF"};
    return component_units(kind, true);
}
// Bound the editor footprint by visible rows, not by the complete circuit size.
// Recalculate on font/style changes so larger system text remains editable.
class DraftTable final : public QTableWidget {
  public:
    DraftTable(QWidget* parent, int columns, int visible_rows)
        : QTableWidget(0, columns, parent), visible_rows_(visible_rows) {
        sizing_connections_ = {
            connect(model(), &QAbstractItemModel::rowsInserted, this, [this] { fit_rows(); }),
            connect(model(), &QAbstractItemModel::rowsRemoved, this, [this] { fit_rows(); }),
            connect(model(), &QAbstractItemModel::modelReset, this, [this] { fit_rows(); })};
    }
    ~DraftTable() override {
        // The base table destroys its headers before its model emits modelReset.
        // Stop our sizing callbacks before that teardown begins.
        for (const auto& connection : sizing_connections_)
            disconnect(connection);
    }
    QSize sizeHint() const override { return {QTableWidget::sizeHint().width(), maximumHeight()}; }
    void fit_rows() {
        QComboBox sample;
        sample.setFont(font());
        const int row = std::max(fontMetrics().height() + 6, sample.sizeHint().height() + 2);
        verticalHeader()->setDefaultSectionSize(row);
        const int height = horizontalHeader()->sizeHint().height() +
                           std::clamp(rowCount(), 1, visible_rows_) * row + 2 * frameWidth() +
                           style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        setFixedHeight(height);
    }

  protected:
    void changeEvent(QEvent* event) override {
        QTableWidget::changeEvent(event);
        if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
            fit_rows();
    }

  private:
    int visible_rows_;
    std::array<QMetaObject::Connection, 3> sizing_connections_;
};
QTableWidget* table(QWidget* parent, const char* name, const QStringList& headers) {
    auto* t = new DraftTable(parent, static_cast<int>(headers.size()),
                             QString::fromUtf8(name) == "transient_nodes" ? 4 : 6);
    t->setObjectName(name);
    t->setAccessibleName(QString::fromUtf8(name).replace('_', ' '));
    t->setHorizontalHeaderLabels(headers);
    t->setTabKeyNavigation(false);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    t->horizontalHeader()->setStretchLastSection(false);
    t->verticalHeader()->hide();
    for (int c = 0; c < headers.size(); ++c) {
        const auto& h = headers[c];
        const int chars = h == "ID"                         ? 7
                          : h == "Unit" || h == "Time unit" ? 9
                          : h == "Name"                     ? 26
                          : h == "Kind"                     ? 19
                                                            : 18;
        t->setColumnWidth(c, t->fontMetrics().horizontalAdvance(QString(chars, '0')) + 16);
        t->horizontalHeaderItem(c)->setToolTip(h);
    }
    t->fit_rows();
    return t;
}
class DraftItem final : public QTableWidgetItem {
  public:
    using QTableWidgetItem::QTableWidgetItem;
    QVariant data(int role) const override {
        if (role == Qt::ToolTipRole)
            return Qt::convertFromPlainText(QTableWidgetItem::data(Qt::DisplayRole).toString());
        return QTableWidgetItem::data(role);
    }
};
void item(QTableWidget* t, int r, int c, const std::string& text, bool editable = true) {
    auto* i = new DraftItem(qt_text(text));
    if (!editable)
        i->setFlags(i->flags() & ~Qt::ItemIsEditable);
    t->setItem(r, c, i);
}
QComboBox* choice(QTableWidget* t, int r, int c, const QStringList& tokens,
                  const std::string& selected) {
    auto* b = new QComboBox(t);
    b->addItems(tokens);
    b->setCurrentText(qt_text(selected));
    b->setAccessibleName(t->horizontalHeaderItem(c)->text() + " row " + QString::number(r + 1));
    b->setToolTip(Qt::convertFromPlainText(b->currentText()));
    QObject::connect(b, &QComboBox::currentTextChanged, b,
                     [b](const QString& text) { b->setToolTip(Qt::convertFromPlainText(text)); });
    t->setCellWidget(r, c, b);
    return b;
}
void fill_ref(QComboBox* b, const std::vector<project::Node>& nodes, project::Reference ref) {
    const QSignalBlocker block(b);
    b->clear();
    b->addItem("Unselected");
    for (const auto& n : nodes)
        b->addItem(qt_text(n.name) + " [" + QString::number(n.id.value) + "]", n.id.value);
    select_reference(b, ref);
    b->setToolTip(Qt::convertFromPlainText(b->currentText()));
}
// Conversion is an explicit user action only. Never invoked by capture or restore.
std::optional<std::string> converted(const std::string& text, const std::string& old_unit,
                                     const QString& new_unit) {
    const auto old = circuit_value_si(qt_text(text), 1);
    if (!old)
        return {};
    auto exponent = [](const QString& token) {
        if (token.startsWith('M'))
            return 6;
        if (token.startsWith('k'))
            return 3;
        if (token.startsWith('m'))
            return -3;
        if (token.startsWith('u'))
            return -6;
        if (token.startsWith('n'))
            return -9;
        if (token.startsWith('p'))
            return -12;
        return 0;
    };
    const double scale = std::pow(10.0, exponent(qt_text(old_unit)) - exponent(new_unit));
    const double value = *old * scale;
    if (!std::isfinite(value) || (*old != 0 && value == 0))
        return {};
    auto out = QString::number(value, 'g', std::numeric_limits<double>::max_digits10);
    if (!circuit_value_si(out, 1))
        return {};
    return draft_text(out);
}
} // namespace
TransientView::TransientView(QWidget* parent, project::TransientDraft* draft)
    : DraftView(parent), state_(draft, project::TransientDraft{}) {
    setObjectName("transient_view");
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop);
    layout->setSpacing(4);
    auto* description = new QLabel(
        "Transient analysis — instantaneous SI voltages/currents; first-order backward Euler.",
        this);
    description->setWordWrap(true);
    description->setTextFormat(Qt::PlainText);
    layout->addWidget(description);
    auto* form = new QFormLayout;
    form->setVerticalSpacing(4);
    layout->addLayout(form);
    auto quantity = [&](const QString& label, const char* name, project::Quantity& q) {
        auto* row = new QWidget(this);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        auto* text = new QLineEdit(row);
        text->setObjectName(name);
        text->setMaxLength(128);
        text->setAccessibleName(label);
        auto* unit = new QComboBox(row);
        unit->setObjectName(QString::fromUtf8(name) + "_unit");
        unit->setAccessibleName(label + " unit");
        unit->addItems(times);
        unit->setCurrentText(qt_text(q.unit));
        h->addWidget(text);
        h->addWidget(unit);
        form->addRow(label, row);
        bind_text(text, q.text);
        connect(
            unit, &QComboBox::currentTextChanged, this,
            [this, unit, text, &q](const QString& next) {
                if (restoring_)
                    return;
                synchronize_pending_text();
                auto result = converted(q.text, q.unit, next);
                if (!result) {
                    const QSignalBlocker block(unit);
                    unit->setCurrentText(qt_text(q.unit));
                    report(
                        "Unit change rejected: keep valid, representable text before converting.");
                    return;
                }
                q = {*result, draft_text(next)};
                {
                    const QSignalBlocker block(text);
                    text->setText(qt_text(q.text));
                }
                edited();
            });
    };
    quantity("Stop time", "transient_stop", state_.get().stop);
    quantity("Maximum integration step", "transient_step", state_.get().maximum_step);
    auto* mode = new QComboBox(this);
    mode->setObjectName("transient_initialization");
    mode->addItems({"Operating point — steady state, not zero energy",
                    "Specified storage — explicit capacitor V / inductor I"});
    mode->setAccessibleName("Transient initialization policy");
    form->addRow("Initialization", mode);
    bind_choice(mode, state_.get().initialization, {"operating_point", "specified_storage"});
    auto* display = new QComboBox(this);
    display->setObjectName("transient_display_unit");
    display->addItems(times);
    display->setAccessibleName("Transient display time unit");
    form->addRow("Display time unit", display);
    bind_choice(display, state_.get().display_time_unit, times);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName("transient_tabs");

    auto page = [&](const QString& title) {
        auto* w = new QWidget(tabs);
        auto* l = new QVBoxLayout(w);
        tabs->addTab(w, title);
        return l;
    };
    auto* editor = page("Circuit");
    auto* node_actions = new QHBoxLayout;
    editor->addLayout(node_actions);
    nodes_ = table(this, "transient_nodes", {"ID", "Name"});
    editor->addWidget(nodes_);
    ground_ = new QComboBox(this);
    ground_->setObjectName("transient_ground");
    ground_->setAccessibleName("Transient ground reference");
    ground_->setMinimumContentsLength(14);
    ground_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    auto actions = [&](QBoxLayout* l, const QString& label, const char* name,
                       std::function<void()> f) {
        auto* b = new QPushButton(label, this);
        b->setObjectName(name);
        l->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, f] {
            synchronize_pending_text();
            f();
        });
        return b;
    };
    actions(node_actions, "Add node", "transient_add_node", [this] {
        auto& d = state_.get();
        if (d.nodes.size() >= project::limits::circuit_nodes) {
            report("Node limit reached.");
            return;
        }
        const auto previous_counter = d.next_node;
        try {
            auto id = project::allocate_id(d.next_node, project::reserved_node_ids(d));
            d.nodes.push_back({id, "N" + std::to_string(id.value)});
            render();
            edited();
        } catch (const project::Error& e) {
            if (d.next_node != previous_counter)
                edited();
            report(QString::fromUtf8(e.what()));
        }
    });
    actions(node_actions, "Remove node", "transient_remove_node", [this] {
        auto r = nodes_->currentRow();
        if (r >= 0) {
            auto& d = state_.get();
            d.nodes.erase(d.nodes.begin() + r);
            render();
            edited();
        }
    });
    auto* ground_label = new QLabel("Ground (0 V)", this);
    ground_label->setBuddy(ground_);
    node_actions->addWidget(ground_label);
    node_actions->addWidget(ground_, 1);
    auto* component_actions = new QHBoxLayout;
    editor->addLayout(component_actions);
    components_ = table(this, "transient_components",
                        {"ID", "Name", "Kind", "Positive", "Negative", "Value", "Unit"});
    editor->addWidget(components_);
    auto* add_kind = new QComboBox(this);
    add_kind->setObjectName("transient_new_kind");
    add_kind->setAccessibleName("New transient component kind");
    for (int i = 0; i < kinds.size(); ++i)
        add_kind->addItem(kind_labels[i], kinds[i]);
    add_kind->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    add_kind->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    auto* kind_label = new QLabel("Component", this);
    kind_label->setBuddy(add_kind);
    component_actions->addWidget(kind_label);
    component_actions->addWidget(add_kind);
    actions(component_actions, "Add component", "transient_add_component", [this, add_kind] {
        auto& d = state_.get();
        if (d.components.size() >= project::limits::circuit_components) {
            report("Component limit reached.");
            return;
        }
        const auto previous_counter = d.next_component;
        try {
            project::TransientComponent c;
            c.id = project::allocate_id(d.next_component, project::reserved_component_ids(d));
            c.kind = draft_text(add_kind->currentData().toString());
            c.name = "P" + std::to_string(c.id.value);
            c.value.unit = draft_text(units(c.kind).front());
            d.components.push_back(c);
            render();
            edited();
        } catch (const project::Error& e) {
            if (d.next_component != previous_counter)
                edited();
            report(QString::fromUtf8(e.what()));
        }
    });
    actions(component_actions, "Remove component", "transient_remove_component", [this] {
        auto r = components_->currentRow();
        if (r >= 0) {
            state_.get().components.erase(state_.get().components.begin() + r);
            render();
            edited();
        }
    });
    component_actions->addStretch();
    QWidget::setTabOrder(add_kind, findChild<QPushButton*>("transient_add_component"));
    QWidget::setTabOrder(findChild<QPushButton*>("transient_add_component"),
                         findChild<QPushButton*>("transient_remove_component"));
    for (auto name : {"transient_remove_node", "transient_remove_component"})
        findChild<QPushButton*>(name)->setToolTip(
            "Remove the selected row. References to its ID are retained until you edit them.");
    auto* sources = page("Sources");
    auto* note =
        new QLabel("Constant mode retains inactive points. Point amplitudes share the component "
                   "value unit. Passive-row source configuration is retained, not executed.",
                   this);
    note->setWordWrap(true);
    note->setTextFormat(Qt::PlainText);
    sources->addWidget(note);
    source_component_ = new QComboBox(this);
    source_component_->setObjectName("transient_source_component");
    source_component_->setAccessibleName("Component source configuration");
    sources->addWidget(source_component_);
    source_mode_ = new QComboBox(this);
    source_mode_->setObjectName("transient_source_mode");
    source_mode_->setAccessibleName("Source interpolation mode");
    source_mode_->addItems({"constant", "hold", "linear"});
    sources->addWidget(source_mode_);
    points_ = table(this, "transient_points", {"Time", "Time unit", "Amplitude (component unit)"});
    sources->addWidget(points_);
    actions(sources, "Add source point", "transient_add_point", [this] {
        if (source_row_ < 0)
            return;
        auto& d = state_.get();
        std::size_t total = 0;
        for (auto& c : d.components)
            total += c.source.points.size();
        auto& p = d.components.at(static_cast<std::size_t>(source_row_)).source.points;
        if (p.size() >= project::limits::transient_source_points ||
            total >= project::limits::transient_total_source_points) {
            report("Source-point storage limit reached (inactive points count too).");
            return;
        }
        p.push_back({});
        render_points();
        edited();
    });
    actions(sources, "Remove selected source point", "transient_remove_point", [this] {
        if (source_row_ >= 0 && points_->currentRow() >= 0) {
            auto& p =
                state_.get().components.at(static_cast<std::size_t>(source_row_)).source.points;
            p.erase(p.begin() + points_->currentRow());
            render_points();
            edited();
        }
    });
    auto* storage = page("Initial conditions");
    auto* explain = new QLabel(
        "Operating point retains but ignores these rows. Specified storage requires exactly one "
        "voltage per capacitor and current per inductor.",
        this);
    explain->setWordWrap(true);
    explain->setTextFormat(Qt::PlainText);
    storage->addWidget(explain);
    initial_ = table(this, "transient_initial_conditions", {"Component", "Kind", "Value", "Unit"});
    storage->addWidget(initial_);
    auto* initial_kind = new QComboBox(this);
    initial_kind->setObjectName("transient_new_initial_kind");
    initial_kind->setAccessibleName("New stored-energy condition kind");
    for (int i = 0; i < initial_kinds.size(); ++i)
        initial_kind->addItem(initial_labels[i], initial_kinds[i]);
    storage->addWidget(initial_kind);
    actions(storage, "Add initial condition", "transient_add_initial", [this, initial_kind] {
        auto& v = state_.get().initial_conditions;
        if (v.size() >= project::limits::transient_initial_conditions) {
            report("Initial-condition limit reached.");
            return;
        }
        project::TransientInitialCondition i;
        i.kind = draft_text(initial_kind->currentData().toString());
        i.value.unit = i.kind == "capacitor_voltage" ? "V" : "A";
        v.push_back(i);
        render();
        edited();
    });
    actions(storage, "Remove selected initial condition", "transient_remove_initial", [this] {
        int r = initial_->currentRow();
        if (r >= 0) {
            auto& v = state_.get().initial_conditions;
            v.erase(v.begin() + r);
            render();
            edited();
        }
    });
    auto* probe = page("Probes");
    probes_ = table(this, "transient_probes",
                    {"Name", "Kind", "Positive node", "Negative node", "Component"});
    probe->addWidget(probes_);
    actions(probe, "Add voltage/current probe", "transient_add_probe", [this] {
        auto& v = state_.get().probes;
        if (v.size() >= project::limits::transient_probes) {
            report("Probe limit reached.");
            return;
        }
        v.push_back({});
        render();
        edited();
    });
    actions(probe, "Remove selected probe", "transient_remove_probe", [this] {
        int r = probes_->currentRow();
        if (r >= 0) {
            auto& v = state_.get().probes;
            v.erase(v.begin() + r);
            render();
            edited();
        }
    });
    auto* help = page("Conventions");
    auto* help_text = new QTextBrowser(this);
    help_text->setObjectName("transient_help");
    help_text->setPlainText(
        "Run creates an owned experiment. Pause/Resume retain it; Step accepts one interval. "
        "Cancel retains the accepted prefix; Reset results leaves the draft unchanged. "
        "Edits make retained results stale and end resumability.\n\nValues are instantaneous "
        "volts/amperes, "
        "not RMS phasors. Current orientation is positive to negative.\nOperating point starts at "
        "steady state; specified storage uses explicit capacitor voltages and inductor "
        "currents.\nIntegration uses first-order backward Euler, with numerical damping. "
        "Source breakpoints preserve storage continuity where physically possible.\n\nComponent "
        "physical kinds are fixed at creation. Add a separate component to change dimension "
        "without reinterpreting retained values. Same-dimension unit conversion is all-or-nothing, "
        "including inactive source amplitudes.\n\nIncomplete text and dangling connections remain "
        "savable. Schema-2 files require this version of OpenECE; older 1.0.x readers reject "
        "them.");
    help->addWidget(help_text);
    status_ = new QLabel("Ready — no simulation results.", this);
    status_->setObjectName("transient_status");
    status_->setWordWrap(true);
    status_->setTextFormat(Qt::PlainText);
    layout->addWidget(status_);
    diagnostic_ = new QLabel(this);
    diagnostic_->setObjectName("transient_diagnostic");
    diagnostic_->setWordWrap(true);
    diagnostic_->setTextFormat(Qt::PlainText);
    diagnostic_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    layout->addWidget(diagnostic_);
    progress_ = new QProgressBar(this);
    progress_->setObjectName("transient_progress");
    progress_->setAccessibleName("Transient accepted time progress");
    progress_->setRange(0, 1000);
    layout->addWidget(progress_);
    auto* controls = new QHBoxLayout;
    layout->addLayout(controls);
    auto button = [&](const QString& text, const char* name) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(name);
        controls->addWidget(b);
        return b;
    };
    run_ = button("Run", "transient_run");
    pause_ = button("Pause", "transient_pause");
    step_ = button("Step", "transient_step_execution");
    cancel_ = button("Cancel", "transient_cancel");
    reset_ = button("Reset results", "transient_reset");
    connect(run_, &QPushButton::clicked, this, [this] { start_execution(false); });
    connect(step_, &QPushButton::clicked, this, [this] { start_execution(true); });
    connect(pause_, &QPushButton::clicked, this, [this] {
        if (!runner_)
            return;
        if (execution_state_ == ExecutionState::paused) {
            runner_->resume();
            execution_state_ = ExecutionState::running;
        } else {
            runner_->pause();
            pending_ = true;
        }
        update_execution_ui();
    });
    connect(cancel_, &QPushButton::clicked, this, [this] {
        if (runner_) {
            runner_->cancel();
            pending_ = true;
            update_execution_ui();
        }
    });
    connect(reset_, &QPushButton::clicked, this, &TransientView::reset_results);
    // Result navigation is transient, never a schema token or persisted edit.
    auto* results = new QTabWidget(this);
    results->setObjectName("transient_results_tabs");
    auto* split = new TransientSplitter(this);
    split->setObjectName("transient_editor_results_split");
    split->setAccessibleName("Resize transient editor and results");
    auto* editor_scroll = new TransientEditorScroll(split);
    editor_scroll->setObjectName("transient_editor_scroll");
    editor_scroll->setAccessibleName("Transient editor, scroll for additional controls");
    editor_scroll->setFrameShape(QFrame::NoFrame);
    editor_scroll->setWidgetResizable(true);
    editor_scroll->setWidget(tabs);
    editor_scroll->setMinimumHeight(6 * split->fontMetrics().height());
    split->addWidget(editor_scroll);
    split->addWidget(results);
    // The editor scrolls instead of clipping its controls at the smaller end.
    // Results retain their content minimum (tab bar, selector, plot and axes).
    // The outer workspace scrolls when these panel minima do not fit.
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setMinimumHeight(300);
    // Other domain pages can make the outer stacked workspace taller than its
    // viewport. Do not stretch this editor/results block into that spare height.
    split->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    layout->addWidget(split);
    auto plot_page = [&](const QString& title, const char* selector_name, QComboBox*& selector,
                         TransientPlot*& plot) {
        auto* w = new QWidget(results);
        auto* l = new QVBoxLayout(w);
        selector = new QComboBox(w);
        selector->setObjectName(selector_name);
        selector->setAccessibleName(title + " trace selection (frozen run metadata)");
        plot = new TransientPlot(w);
        l->addWidget(selector);
        l->addWidget(plot);
        results->addTab(w, title);
        connect(selector, &QComboBox::currentIndexChanged, this, &TransientView::update_plots);
    };
    plot_page("Voltages (V)", "transient_voltage_trace", voltage_select_, voltage_plot_);
    plot_page("Currents (A)", "transient_current_trace", current_select_, current_plot_);
    voltage_plot_->setObjectName("transient_voltage_plot");
    current_plot_->setObjectName("transient_current_plot");
    auto* trace = new QTableView(results);
    trace->setObjectName("transient_trace");
    trace->setAccessibleName("Accepted numerical trace, mixed probe declaration order");
    trace_model_ = new TransientTraceModel(trace);
    trace->setModel(trace_model_);
    trace->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    trace->horizontalHeader()->setDefaultSectionSize(
        trace->fontMetrics().horizontalAdvance(QString(28, '0')) + 16);
    trace->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    trace->horizontalHeader()->setMinimumHeight(2 * trace->fontMetrics().height() + 12);
    trace->setWordWrap(false);
    trace->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    trace->horizontalHeader()->setStretchLastSection(false);
    auto size_time_columns = [trace] {
        trace->setColumnWidth(0, trace->fontMetrics().horizontalAdvance(QString(19, '0')) + 16);
        trace->setColumnWidth(1, trace->fontMetrics().horizontalAdvance("before breakpoint") + 24);
    };
    size_time_columns();
    connect(trace->horizontalHeader(), &QHeaderView::sectionCountChanged, trace,
            [size_time_columns] { size_time_columns(); });
    results->addTab(trace, "Numerical trace");
    poll_ = new QTimer(this);
    poll_->setInterval(50);
    connect(poll_, &QTimer::timeout, this, &TransientView::receive_update);
    connect(this, &DraftView::draftEdited, this, &TransientView::invalidate_execution);
    update_execution_ui();
    for (auto* t : {nodes_, components_, points_, initial_, probes_}) {
        bind_table(t, [this, t](int r, int c, const QString& text) { text_edit(t, r, c, text); });
        connect(t, &QTableWidget::itemChanged, this,
                [this, t](QTableWidgetItem* i) { text_edit(t, i->row(), i->column(), i->text()); });
    }
    connect(ground_, &QComboBox::currentIndexChanged, this, [this] {
        if (!rendering_ && !restoring_)
            edit(state_.get().ground, selected_reference(ground_));
    });
    connect(source_component_, &QComboBox::currentIndexChanged, this, [this](int row) {
        if (rendering_)
            return;
        synchronize_pending_text();
        source_row_ = row;
        render_points();
    });
    connect(source_mode_, &QComboBox::currentTextChanged, this, [this](const QString& m) {
        if (!rendering_ && !restoring_ && source_row_ >= 0)
            edit(state_.get().components.at(static_cast<std::size_t>(source_row_)).source.mode,
                 draft_text(m));
    });
    render();
    split->setSizes({editor_scroll->sizeHint().height(), results->sizeHint().height()});
    bind_tabs(tabs, state_.get().selected_tab,
              {"editor", "sources", "initial_conditions", "probes", "help"});
    restoring_ = false;
}
void TransientView::report(const QString& text) { status_->setText(text); }
void TransientView::synchronize_pending_text() { DraftView::synchronize_pending_text(); }
void TransientView::text_edit(QTableWidget* t, int r, int c, const QString& value) {
    if (rendering_ || restoring_ || r < 0)
        return;
    auto& d = state_.get();
    std::string* target = nullptr;
    auto row = static_cast<std::size_t>(r);
    if (t == nodes_ && c == 1 && row < d.nodes.size())
        target = &d.nodes[row].name;
    if (t == components_ && row < d.components.size()) {
        if (c == 1)
            target = &d.components[row].name;
        if (c == 5)
            target = &d.components[row].value.text;
    }
    if (t == points_ && source_row_ >= 0) {
        auto& v = d.components.at(static_cast<std::size_t>(source_row_)).source.points;
        if (row < v.size()) {
            if (c == 0)
                target = &v[row].time.text;
            if (c == 2)
                target = &v[row].value_text;
        }
    }
    if (t == initial_ && c == 2 && row < d.initial_conditions.size())
        target = &d.initial_conditions[row].value.text;
    if (t == probes_ && c == 0 && row < d.probes.size())
        target = &d.probes[row].name;
    if (target && *target != draft_text(value)) {
        *target = draft_text(value);
        edited();
    }
    // Update labels without replacing table editors or touching references.
    if (target && ((t == nodes_ && c == 1) || (t == components_ && c == 1))) {
        refresh_references();
        if (t == components_)
            source_component_->setItemText(r, qt_text(d.components[row].name) + " [" +
                                                  QString::number(d.components[row].id.value) +
                                                  "]");
    }
}
void TransientView::refresh_references() {
    auto& d = state_.get();
    fill_ref(ground_, d.nodes, d.ground);
    auto refs = [&](QTableWidget* t, int r, int c, project::Reference& ref, const auto& rows) {
        auto* b = static_cast<QComboBox*>(t->cellWidget(r, c));
        fill_ref(b, rows, ref);
    };
    for (int r = 0; r < components_->rowCount(); ++r) {
        auto& c = d.components[static_cast<std::size_t>(r)];
        refs(components_, r, 3, c.positive, d.nodes);
        refs(components_, r, 4, c.negative, d.nodes);
    }
    std::vector<project::Node> parts;
    for (auto& c : d.components)
        parts.push_back({c.id, c.name});
    for (int r = 0; r < initial_->rowCount(); ++r)
        refs(initial_, r, 0, d.initial_conditions[static_cast<std::size_t>(r)].component, parts);
    for (int r = 0; r < probes_->rowCount(); ++r) {
        auto& p = d.probes[static_cast<std::size_t>(r)];
        refs(probes_, r, 2, p.positive, d.nodes);
        refs(probes_, r, 3, p.negative, d.nodes);
        refs(probes_, r, 4, p.component, parts);
    }
}
void TransientView::change_component_unit(int r, QComboBox* box) {
    if (rendering_ || restoring_)
        return;
    synchronize_pending_text();
    auto& c = state_.get().components.at(static_cast<std::size_t>(r));
    auto next = box->currentText();
    auto value = converted(c.value.text, c.value.unit, next);
    std::vector<std::string> values;
    for (auto& p : c.source.points) {
        auto v = converted(p.value_text, c.value.unit, next);
        if (!v) {
            value.reset();
            break;
        }
        values.push_back(*v);
    }
    if (!value) {
        const QSignalBlocker block(box);
        box->setCurrentText(qt_text(c.value.unit));
        box->setToolTip(Qt::convertFromPlainText(box->currentText()));
        report("Unit change rejected: component and ALL retained point amplitudes must be valid "
               "and representable. Nothing was converted.");
        return;
    }
    c.value = {*value, draft_text(next)};
    for (std::size_t i = 0; i < values.size(); ++i)
        c.source.points[i].value_text = values[i];
    {
        const QSignalBlocker block(components_);
        components_->item(r, 5)->setText(qt_text(c.value.text));
    }
    render_points();
    edited();
}
void TransientView::render() {
    QScopedValueRollback guard(rendering_, true);
    auto& d = state_.get();
    for (auto* t : {nodes_, components_, initial_, probes_})
        t->setRowCount(0);
    auto ref = [&](QTableWidget* t, int r, int c, auto update) {
        auto* b = choice(t, r, c, {}, "");
        connect(b, &QComboBox::currentIndexChanged, this, [this, b, update] {
            if (!rendering_ && !restoring_)
                update(selected_reference(b));
        });
    };
    for (int r = 0; r < static_cast<int>(d.nodes.size()); ++r) {
        nodes_->insertRow(r);
        item(nodes_, r, 0, std::to_string(d.nodes[static_cast<std::size_t>(r)].id.value), false);
        item(nodes_, r, 1, d.nodes[static_cast<std::size_t>(r)].name);
    }
    for (int r = 0; r < static_cast<int>(d.components.size()); ++r) {
        auto& c = d.components[static_cast<std::size_t>(r)];
        components_->insertRow(r);
        item(components_, r, 0, std::to_string(c.id.value), false);
        item(components_, r, 1, c.name);
        auto* kind = choice(
            components_, r, 2, kind_labels,
            kind_labels.value(kinds.indexOf(qt_text(c.kind)), qt_text(c.kind)).toStdString());
        for (int i = 0; i < kinds.size(); ++i)
            kind->setItemData(i, kinds[i]);
        connect(kind, &QComboBox::currentTextChanged, this, [this, kind, r](const QString&) {
            if (rendering_ || restoring_)
                return;
            auto& current = state_.get().components.at(static_cast<std::size_t>(r));
            if (kind->currentData().toString() != qt_text(current.kind)) {
                const QSignalBlocker b(kind);
                kind->setCurrentIndex(static_cast<int>(kinds.indexOf(qt_text(current.kind))));
                kind->setToolTip(Qt::convertFromPlainText(kind->currentText()));
                report("Component kind has a different physical dimension. Add a separate "
                       "component; this row and its source points are kept unchanged.");
            }
        });
        ref(components_, r, 3, [this, r](auto v) {
            edit(state_.get().components.at(static_cast<std::size_t>(r)).positive, v);
        });
        ref(components_, r, 4, [this, r](auto v) {
            edit(state_.get().components.at(static_cast<std::size_t>(r)).negative, v);
        });
        item(components_, r, 5, c.value.text);
        auto* unit = choice(components_, r, 6, units(c.kind), c.value.unit);
        connect(unit, &QComboBox::currentTextChanged, this,
                [this, r, unit] { change_component_unit(r, unit); });
    }
    for (int r = 0; r < static_cast<int>(d.initial_conditions.size()); ++r) {
        auto& i = d.initial_conditions[static_cast<std::size_t>(r)];
        initial_->insertRow(r);
        ref(initial_, r, 0, [this, r](auto v) {
            edit(state_.get().initial_conditions.at(static_cast<std::size_t>(r)).component, v);
        });
        item(initial_, r, 1,
             draft_text(
                 initial_labels.value(initial_kinds.indexOf(qt_text(i.kind)), qt_text(i.kind))),
             false);
        item(initial_, r, 2, i.value.text);
        auto* u = choice(initial_, r, 3,
                         i.kind == "capacitor_voltage" ? units("voltage_source")
                                                       : units("current_source"),
                         i.value.unit);
        connect(u, &QComboBox::currentTextChanged, this, [this, r, u] {
            if (rendering_ || restoring_)
                return;
            synchronize_pending_text();
            auto& q = state_.get().initial_conditions.at(static_cast<std::size_t>(r)).value;
            auto v = converted(q.text, q.unit, u->currentText());
            if (!v) {
                const QSignalBlocker b(u);
                u->setCurrentText(qt_text(q.unit));
                u->setToolTip(Qt::convertFromPlainText(u->currentText()));
                report("Initial-value unit conversion rejected; text kept.");
                return;
            }
            q = {*v, draft_text(u->currentText())};
            {
                const QSignalBlocker b(initial_);
                initial_->item(r, 2)->setText(qt_text(q.text));
            }
            edited();
        });
    }
    for (int r = 0; r < static_cast<int>(d.probes.size()); ++r) {
        auto& p = d.probes[static_cast<std::size_t>(r)];
        probes_->insertRow(r);
        item(probes_, r, 0, p.name);
        auto* kind = choice(probes_, r, 1, {"voltage", "current"}, p.kind);
        connect(kind, &QComboBox::currentTextChanged, this, [this, r](const QString& s) {
            if (!rendering_ && !restoring_)
                edit(state_.get().probes.at(static_cast<std::size_t>(r)).kind, draft_text(s));
        });
        ref(probes_, r, 2, [this, r](auto v) {
            edit(state_.get().probes.at(static_cast<std::size_t>(r)).positive, v);
        });
        ref(probes_, r, 3, [this, r](auto v) {
            edit(state_.get().probes.at(static_cast<std::size_t>(r)).negative, v);
        });
        ref(probes_, r, 4, [this, r](auto v) {
            edit(state_.get().probes.at(static_cast<std::size_t>(r)).component, v);
        });
    }
    refresh_references();
    {
        const QSignalBlocker b(source_component_);
        source_component_->clear();
        for (auto& c : d.components)
            source_component_->addItem(qt_text(c.name) + " [" + QString::number(c.id.value) + "]");
        source_row_ = d.components.empty()
                          ? -1
                          : std::clamp(source_row_, 0, static_cast<int>(d.components.size()) - 1);
        source_component_->setCurrentIndex(source_row_);
    }
    render_points();
}
void TransientView::render_points() {
    QScopedValueRollback guard(rendering_, true);
    points_->setRowCount(0);
    source_mode_->setEnabled(source_row_ >= 0);
    if (source_row_ < 0)
        return;
    auto& selected = state_.get().components.at(static_cast<std::size_t>(source_row_));
    auto& source = selected.source;
    points_->horizontalHeaderItem(2)->setText("Amplitude [" + qt_text(selected.value.unit) + "]");
    source_mode_->setCurrentText(qt_text(source.mode));
    for (int r = 0; r < static_cast<int>(source.points.size()); ++r) {
        auto& p = source.points[static_cast<std::size_t>(r)];
        points_->insertRow(r);
        item(points_, r, 0, p.time.text);
        item(points_, r, 2, p.value_text);
        auto* u = choice(points_, r, 1, times, p.time.unit);
        connect(u, &QComboBox::currentTextChanged, this, [this, r, u] {
            if (rendering_ || restoring_ || source_row_ < 0)
                return;
            synchronize_pending_text();
            auto& q = state_.get()
                          .components.at(static_cast<std::size_t>(source_row_))
                          .source.points.at(static_cast<std::size_t>(r))
                          .time;
            auto v = converted(q.text, q.unit, u->currentText());
            if (!v) {
                const QSignalBlocker b(u);
                u->setCurrentText(qt_text(q.unit));
                u->setToolTip(Qt::convertFromPlainText(u->currentText()));
                report("Point-time unit conversion rejected; text kept.");
                return;
            }
            q = {*v, draft_text(u->currentText())};
            {
                const QSignalBlocker b(points_);
                points_->item(r, 0)->setText(qt_text(q.text));
            }
            edited();
        });
    }
}

TransientView::~TransientView() {
    poll_->stop();
    runner_.reset();
}
void TransientView::start_execution(bool single) {
    synchronize_pending_text();
    if (single && runner_ && execution_state_ == ExecutionState::paused && !pending_) {
        pending_ = true;
        runner_->step();
        update_execution_ui();
        return;
    }
    reset_results();
    // Freeze the attempted raw configuration even if validation rejects it.
    run_draft_ = transient_active_draft(state_.get());
    try {
        auto execution = transient_execution(state_.get());
        labels_ = execution.probes;
        time_unit_ = execution.time_unit;
        time_scale_ = execution.time_scale;
        stop_seconds_ = execution.request.stop_seconds;
        for (std::size_t i = 0; i < labels_.size(); ++i) {
            auto* select = labels_[i].voltage ? voltage_select_ : current_select_;
            select->addItem(labels_[i].label, static_cast<int>(i));
        }
        runner_ = std::make_unique<TransientRunner>(std::move(execution), single);
        execution_state_ = ExecutionState::running;
        pending_ = single;
        poll_->start();
    } catch (const transient_core::Error& e) {
        execution_state_ = ExecutionState::failed;
        diagnostic_->setText("Configuration rejected: " + QString::fromUtf8(e.what()));
    } catch (const std::exception&) {
        execution_state_ = ExecutionState::failed;
        diagnostic_->setText("Unable to prepare the experiment or start the numerical worker.");
    }
    update_execution_ui();
}
void TransientView::receive_update() {
    if (!runner_)
        return;
    const auto update = runner_->take_update();
    if (!update)
        return;
    result_ = update->result;
    using M = TransientRunner::Mode;
    switch (update->mode) {
    case M::running:
        execution_state_ = ExecutionState::running;
        break;
    case M::paused:
        execution_state_ = ExecutionState::paused;
        break;
    case M::complete:
        execution_state_ = ExecutionState::complete;
        break;
    case M::cancelled:
        execution_state_ = ExecutionState::cancelled;
        break;
    case M::failed:
        execution_state_ = ExecutionState::failed;
        break;
    }
    pending_ = false;
    if (!update->error.isEmpty())
        diagnostic_->setText(update->error);
    if (result_ && result_->failure) {
        const auto& failure = *result_->failure;
        diagnostic_->setText(
            "Attempted failure time: " + QString::number(failure.time_seconds, 'g', 17) +
            " s; last accepted time: " + QString::number(result_->current_time_seconds, 'g', 17) +
            " s.\n" + QString::fromUtf8(failure.error.what()));
    }
    trace_model_->set_result(result_, labels_, time_unit_, time_scale_);
    update_plots();
    update_execution_ui();
    if (update->mode == M::complete || update->mode == M::cancelled || update->mode == M::failed) {
        poll_->stop();
        runner_.reset();
    }
}
void TransientView::update_execution_ui() {
    diagnostic_->setVisible(!diagnostic_->text().isEmpty());
    const bool running = execution_state_ == ExecutionState::running;
    const bool paused = execution_state_ == ExecutionState::paused;
    const bool resumable = bool(runner_);
    run_->setEnabled(!running && !pending_);
    pause_->setEnabled(resumable && !pending_ && (running || paused));
    pause_->setText(paused ? "Resume" : "Pause");
    step_->setEnabled(!running && !pending_);
    cancel_->setEnabled(resumable && (running || paused));
    QString status;
    switch (execution_state_) {
    case ExecutionState::ready:
        status = "Ready — no simulation results.";
        break;
    case ExecutionState::running:
        status = !result_   ? "Running — initializing; no accepted trace yet."
                 : pending_ ? "Running — command pending; retaining accepted prefix."
                            : "Running — partial accepted trace.";
        break;
    case ExecutionState::paused:
        status = pending_ ? "Paused — one interval pending."
                          : "Paused — partial accepted trace; Resume or Step.";
        break;
    case ExecutionState::complete:
        status = "Complete — accepted trace matches the frozen run inputs.";
        break;
    case ExecutionState::cancelled:
        status = "Cancelled — partial accepted trace retained; cannot resume.";
        break;
    case ExecutionState::failed:
        status = result_ ? "Failed — partial accepted trace retained."
                         : "Failed — no accepted simulation result.";
        break;
    case ExecutionState::stale:
        status = result_
                     ? "Stale — retained trace belongs to previous inputs; cannot resume."
                     : "Stale — previous diagnostic belongs to earlier inputs; no accepted trace.";
        break;
    }
    if (result_)
        status += " Last accepted: " +
                  QString::number(result_->current_time_seconds / time_scale_, 'g', 12) + " " +
                  time_unit_ + "; " + QString::number(result_->times.size()) + " ordered samples.";
    status_->setText(status);
    progress_->setValue(
        result_ ? static_cast<int>(
                      std::clamp(result_->current_time_seconds / stop_seconds_, 0.0, 1.0) * 1000)
                : 0);
}
void TransientView::update_plots() {
    auto update = [&](QComboBox* selector, TransientPlot* plot) {
        if (!result_ || selector->currentIndex() < 0) {
            plot->clear();
            return;
        }
        const auto index = static_cast<std::size_t>(selector->currentData().toInt());
        plot->show_trace(*result_, labels_.at(index), time_unit_, time_scale_);
    };
    update(voltage_select_, voltage_plot_);
    update(current_select_, current_plot_);
}
void TransientView::reset_results() {
    poll_->stop();
    runner_.reset();
    result_.reset();
    labels_.clear();
    pending_ = false;
    {
        const QSignalBlocker a(voltage_select_), b(current_select_);
        voltage_select_->clear();
        current_select_->clear();
    }
    diagnostic_->clear();
    trace_model_->set_result({}, {}, "s", 1);
    execution_state_ = ExecutionState::ready;
    update_plots();
    update_execution_ui();
}
void TransientView::invalidate_execution() {
    if (execution_state_ == ExecutionState::ready || execution_state_ == ExecutionState::stale)
        return;
    if (transient_active_draft(state_.get()) == run_draft_)
        return;
    poll_->stop();
    runner_.reset();
    pending_ = false;
    execution_state_ = ExecutionState::stale;
    update_execution_ui();
}
void TransientView::stop_execution() {
    if (!runner_)
        return;
    runner_->cancel();
    // Join before session destruction, then collect the final accepted prefix.
    while (runner_) {
        receive_update();
        if (runner_)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
} // namespace openece::gui
