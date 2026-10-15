#include "canvas.hpp"
#include <QEvent>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <functional>

namespace openece::schematic::qt {
namespace {
const QColor background("#1d2229"), foreground("#e3e8f0"), wireColor("#aebfce"),
    highlight("#96aef2"), warning("#f2bb72");
QString literalTooltip(const QString& s) { return "<pre>" + s.toHtmlEscaped() + "</pre>"; }
class NavigationView final : public QGraphicsView {
  public:
    using QGraphicsView::QGraphicsView;
    void zoom(qreal factor) {
        const auto current = transform().m11();
        const auto next = std::clamp(current * factor, .0001, 8.0);
        scale(next / current, next / current);
    }

  protected:
    void wheelEvent(QWheelEvent* event) override {
        if (event->modifiers() & Qt::ControlModifier) {
            if (event->angleDelta().y() != 0)
                zoom(event->angleDelta().y() > 0 ? 1.2 : 1 / 1.2);
            event->accept();
        } else
            QGraphicsView::wheelEvent(event);
    }
};
class PickPath final : public QGraphicsPathItem {
  public:
    PickPath(QPainterPath path, std::function<void()> callback, bool component)
        : QGraphicsPathItem(std::move(path)), callback_(std::move(callback)),
          component_(component) {
        setCursor(Qt::PointingHandCursor);
    }
    QPainterPath shape() const override {
        if (component_) {
            QPainterPath area;
            area.addRect(path().boundingRect());
            return area;
        }
        QPainterPathStroker stroker;
        stroker.setWidth(8);
        stroker.setCapStyle(Qt::FlatCap);
        return stroker.createStroke(path());
    }

  protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            callback_();
            event->accept();
        } else
            event->ignore();
    }

  private:
    std::function<void()> callback_;
    bool component_;
};
QString componentDetails(const Component& c) {
    QString text = QString("Component [%1] — %2\nKind: %3\nPositive terminal: %4\nNegative "
                           "terminal: %5\nBranch current: positive → negative\nRaw value: ")
                       .arg(c.key.value)
                       .arg(QString::fromStdString(c.name), QString::fromStdString(c.kind),
                            reference_text(c.positive), reference_text(c.negative));
    text += QString::fromStdString(c.value.text) +
            "\nUnit token: " + QString::fromStdString(c.value.unit);
    if (c.phase)
        text += "\nRetained raw phase: " + QString::fromStdString(c.phase->text) +
                "\nPhase unit token: " + QString::fromStdString(c.phase->unit);
    if (c.source) {
        text += "\nRetained source mode: " + QString::fromStdString(c.source->mode);
        text += "\nRetained source points (including inactive configuration):";
        for (const auto& p : c.source->points)
            text += "\n  time: " + QString::fromStdString(p.time.text) + " [" +
                    QString::fromStdString(p.time.unit) +
                    "]; value: " + QString::fromStdString(p.value_text);
    }
    return text;
}
} // namespace
struct SchematicCanvas::State {
    std::optional<SchematicGraph> graph;
    std::optional<LayoutDiagram> diagram;
    std::optional<LabelOwner> selection;
    std::vector<std::pair<LabelOwner, PickPath*>> picks;
    std::vector<std::pair<LabelOwner, QTreeWidgetItem*>> rows;
};
SchematicCanvas::SchematicCanvas(QWidget* parent)
    : QWidget(parent), state_(std::make_unique<State>()), scene_(new QGraphicsScene(this)) {
    setObjectName("schematicCanvas");
    auto* layout = new QVBoxLayout(this);
    auto* tools = new QHBoxLayout;
    std::vector<QWidget*> focusOrder;
    for (const auto& entry : {std::pair{"Fit to View", &SchematicCanvas::fit},
                              {"Zoom In", &SchematicCanvas::zoomIn},
                              {"Zoom Out", &SchematicCanvas::zoomOut}}) {
        auto* button = new QPushButton(entry.first, this);
        button->setAccessibleName(entry.first);
        connect(button, &QPushButton::clicked, this, entry.second);
        tools->addWidget(button);
        focusOrder.push_back(button);
    }
    tools->addStretch();
    layout->addLayout(tools);
    // Keep child creation in visual focus order too, including when a host adds
    // more controls after constructing this canvas.
    view_ = new NavigationView(scene_, this);
    status_ = new QLabel(this);
    entities_ = new QTreeWidget(this);
    inspector_ = new QPlainTextEdit(this);
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    auto* split = new QSplitter(this);
    split->setChildrenCollapsible(false);
    split->addWidget(view_);
    auto* details = new QWidget(split);
    auto* detailLayout = new QVBoxLayout(details);
    entities_->setHeaderLabel("Components and electrical nets");
    entities_->setAccessibleName("Schematic entities — select to inspect and highlight");
    inspector_->setReadOnly(true);
    inspector_->setTabChangesFocus(true);
    inspector_->setAccessibleName("Schematic details (plain text)");
    detailLayout->addWidget(entities_, 1);
    detailLayout->addWidget(inspector_, 2);
    split->addWidget(details);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    split->setSizes({850, 300});
    layout->addWidget(split, 1);
    view_->setObjectName("schematicViewport");
    view_->setAccessibleName("Read-only circuit schematic");
    view_->setAccessibleDescription(
        "Ctrl+wheel or plus/minus to zoom; F to fit; drag background to pan. Arrow keys scroll. "
        "Use the entity list for textual inspection.");
    view_->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    view_->setBackgroundBrush(background);
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view_->setFocusPolicy(Qt::StrongFocus);
    // Establish visual order after scroll-area reparenting, independent of the host's
    // construction order. Qt handles each scroll area's viewport focus delegation.
    focusOrder.insert(focusOrder.end(), {view_, entities_, inspector_});
    for (std::size_t i = 1; i < focusOrder.size(); ++i)
        QWidget::setTabOrder(focusOrder[i - 1], focusOrder[i]);
    auto shortcut = [&](QKeySequence key, auto method) {
        auto* action = new QShortcut(key, this);
        action->setContext(Qt::WidgetWithChildrenShortcut);
        connect(action, &QShortcut::activated, this, method);
    };
    shortcut(QKeySequence("F"), &SchematicCanvas::fit);
    shortcut(QKeySequence("+"), &SchematicCanvas::zoomIn);
    shortcut(QKeySequence("-"), &SchematicCanvas::zoomOut);
    connect(entities_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* row) {
        for (const auto& [key, item] : state_->rows)
            if (row == item) {
                select(key);
                break;
            }
    });
    showFailure("No schematic loaded.");
}
SchematicCanvas::~SchematicCanvas() {
    // Quiesce child selection notifications before member-owned lookup data dies.
    entities_->blockSignals(true);
    state_->rows.clear();
    state_->picks.clear();
    scene_->clear();
}
bool SchematicCanvas::setGraph(const SchematicGraph& graph) {
    const auto measured = measure_labels(graph, font());
    const auto result = layout_schematic(graph, measured.metrics);
    if (const auto* error = std::get_if<LayoutFailure>(&result)) {
        showFailure(QString::fromStdString(error->message));
        return false;
    }
    return setDiagram(graph, std::get<LayoutDiagram>(result), measured.metrics);
}
bool SchematicCanvas::setDiagram(const SchematicGraph& graph, const LayoutDiagram& d,
                                 const LayoutMetrics& metrics) {
    const auto measured = measure_labels(graph, font());
    const auto verified = verify_layout(graph, d, metrics);
    if (!verified.valid() || metrics != measured.metrics) {
        showFailure(!verified.valid() ? QString::fromStdString(verified.failure->message)
                                      : "Label measurements do not match the current canvas font.");
        return false;
    }
    install(graph, d, measured);
    return true;
}
void SchematicCanvas::showFailure(const QString& message) {
    entities_->blockSignals(true);
    entities_->clear();
    entities_->blockSignals(false);
    state_->picks.clear();
    state_->rows.clear();
    scene_->clear();
    *state_ = State{};
    inspector_->setPlainText(message);
    status_->setText("Schematic unavailable — " + message);
}
void SchematicCanvas::install(const SchematicGraph& graph, const LayoutDiagram& incoming,
                              const MeasuredLabels& measured) {
    // Callers may pass the current borrowed diagram() back into this method.
    // Finish owning the replacement before destroying any previous scene/state.
    auto ownedGraph = graph;
    auto ownedDiagram = incoming;
    showFailure({});
    state_->graph = std::move(ownedGraph);
    state_->diagram = std::move(ownedDiagram);
    const auto& d = *state_->diagram;
    const auto tooltip = [](const QString& text) { return literalTooltip(text); };
    auto addPick = [&](LabelOwner key, const QPainterPath& path, QColor color, const QString& text,
                       bool filled = false) {
        auto* item = new PickPath(
            path, [this, key] { select(key); }, std::holds_alternative<ComponentKey>(key));
        item->setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (filled)
            item->setBrush(color);
        item->setToolTip(tooltip(text));
        scene_->addItem(item);
        state_->picks.push_back({key, item});
    };
    const auto paths = wire_geometry(d);
    for (const auto& n : d.nets) {
        QPainterPath path;
        for (std::size_t i = 0; i < d.wires.size(); ++i)
            if (d.wires[i].net == n.source.key)
                path.addPath(paths[i]);
        const auto text = QString::fromStdString(node_label_text(n.source));
        addPick(n.source.key, path, wireColor, text);
        QPainterPath dots;
        for (const auto& j : d.junctions)
            if (j.net == n.source.key)
                dots.addEllipse(point(j.at), 2.5, 2.5);
        dots.addEllipse(point(n.anchor), 2.5, 2.5);
        addPick(n.source.key, dots, wireColor, text, true);
    }
    for (const auto& c : d.components)
        addPick(c.source.key, symbol_geometry(c).path, foreground, componentDetails(c.source));
    QPainterPath openMarkers, missingMarkers;
    for (const auto& t : d.terminals) {
        const auto a = point(t.anchor);
        const auto r = static_cast<qreal>(d.marker_radius);
        if (std::holds_alternative<Unselected>(t.reference))
            openMarkers.addEllipse(a, r, r);
        if (std::holds_alternative<DanglingNode>(t.reference)) {
            missingMarkers.moveTo(a + QPointF(-r, -r));
            missingMarkers.lineTo(a + QPointF(r, r));
            missingMarkers.moveTo(a + QPointF(-r, r));
            missingMarkers.lineTo(a + QPointF(r, -r));
        }
    }
    scene_->addPath(openMarkers, QPen(warning, 2), background);
    scene_->addPath(missingMarkers, QPen(warning, 2));
    if (d.ground) {
        const auto anchor = point(d.ground->anchor);
        const auto box = rectangle(d.ground->body);
        QPainterPath path;
        path.moveTo(anchor);
        path.lineTo(QPointF(anchor.x(), box.center().y()));
        for (int i = 0; i < 3; ++i) {
            const auto half = box.width() * (.5 - static_cast<qreal>(i) * .14);
            const auto y = box.center().y() + static_cast<qreal>(i) * box.height() / 5;
            path.moveTo(anchor.x() - half, y);
            path.lineTo(anchor.x() + half, y);
        }
        addPick(d.ground->net, path, wireColor,
                "Explicit ground: " + QString::number(d.ground->net.value));
    }
    for (const auto& label : d.labels) {
        const auto found =
            std::find_if(measured.labels.begin(), measured.labels.end(), [&](const auto& m) {
                return m.owner == label.owner && m.role == label.role;
            });
        auto* text = scene_->addSimpleText(found->text, font());
        text->setBrush(foreground);
        const auto ink = text->boundingRect();
        text->setPos(point(label.bounds.origin) + QPointF(4 - std::min(0.0, ink.left()), 4));
        text->setToolTip(tooltip(QString::fromStdString(label.text)));
        text->setAcceptedMouseButtons(Qt::NoButton);
    }
    entities_->blockSignals(true);
    auto* components = new QTreeWidgetItem(entities_, QStringList{"Components"});
    auto* nets = new QTreeWidgetItem(entities_, QStringList{"Electrical nets"});
    for (const auto& c : d.components) {
        const auto text =
            QString::fromStdString(component_label_text(c.source, LabelRole::component_name));
        auto* row = new QTreeWidgetItem(components, QStringList{text});
        row->setToolTip(0, tooltip(text));
        state_->rows.push_back({c.source.key, row});
    }
    for (const auto& n : d.nets) {
        const auto text = QString::fromStdString(node_label_text(n.source));
        auto* row = new QTreeWidgetItem(nets, QStringList{text});
        row->setToolTip(0, tooltip(text));
        state_->rows.push_back({n.source.key, row});
    }
    entities_->expandAll();
    entities_->blockSignals(false);
    const auto strategy =
        d.strategy == LayoutStrategy::structured
            ? "Structured schematic"
            : "Connectivity layout — labeled net rails (not a textbook arrangement)";
    status_->setText(analysis_name(d.analysis) + " — " + strategy +
                     QString("; %1 components, %2 nets, %3 nonconnecting crossings")
                         .arg(d.components.size())
                         .arg(d.nets.size())
                         .arg(d.crossings.size()));
    QString summary = status_->text() +
                      "\nRead-only. Open circles: unselected terminals. Crosses: dangling "
                      "references. Wire gaps: no electrical connection.\nGround: " +
                      reference_text(d.ground_selection);
    for (const auto& diagnostic : d.diagnostics)
        summary += "\n" + QString::fromStdString(diagnostic);
    inspector_->setPlainText(summary);
    scene_->setSceneRect(rectangle(d.bounds).adjusted(-16, -16, 16, 16));
    fit();
}
bool SchematicCanvas::select(const LabelOwner& key) {
    if (!state_->diagram)
        return false;
    const auto found = std::find_if(state_->rows.begin(), state_->rows.end(),
                                    [&](const auto& row) { return row.first == key; });
    if (found == state_->rows.end())
        return false;
    state_->selection = key;
    for (const auto& [owner, item] : state_->picks) {
        auto pen = item->pen();
        pen.setColor(owner == key                             ? highlight
                     : std::holds_alternative<NodeKey>(owner) ? wireColor
                                                              : foreground);
        pen.setWidthF(owner == key ? 2.8 : 1.7);
        item->setPen(pen);
        if (item->brush().style() != Qt::NoBrush)
            item->setBrush(pen.color());
    }
    entities_->blockSignals(true);
    entities_->setCurrentItem(found->second);
    entities_->blockSignals(false);
    const auto& d = *state_->diagram;
    QString text = analysis_name(d.analysis) + "\n";
    if (const auto* ckey = std::get_if<ComponentKey>(&key)) {
        for (const auto& c : d.components)
            if (c.source.key == *ckey)
                text += componentDetails(c.source);
    } else {
        const auto nkey = std::get<NodeKey>(key);
        for (const auto& n : d.nets)
            if (n.source.key == nkey)
                text += QString("Electrical node [%1] — ").arg(nkey.value) +
                        QString::fromStdString(n.source.name);
        text += "\nConnected component terminals:";
        for (const auto& t : d.terminals)
            if (const auto* ref = std::get_if<DeclaredNode>(&t.reference); ref && ref->key == nkey)
                text += QString("\n  [%1] %2")
                            .arg(t.key.component.value)
                            .arg(t.key.side == TerminalSide::positive ? "positive" : "negative");
        text += "\nJunctions (wire indices are layout-local):";
        for (const auto& j : d.junctions)
            if (j.net == nkey) {
                text += QString("\n  (%1, %2) wires:").arg(j.at.x).arg(j.at.y);
                for (const auto index : j.wires)
                    text += " " + QString::number(index);
            }
        text += "\nNonconnecting crossings (second wire passes over first):";
        for (const auto& c : d.crossings)
            if (d.wires[c.first_wire].net == nkey || d.wires[c.second_wire].net == nkey)
                text += QString("\n  (%1, %2): wire %3 / wire %4; no connection")
                            .arg(c.at.x)
                            .arg(c.at.y)
                            .arg(c.first_wire)
                            .arg(c.second_wire);
    }
    for (const auto& diagnostic : d.diagnostics)
        text += "\nDiagnostic: " + QString::fromStdString(diagnostic);
    inspector_->setPlainText(text);
    return true;
}
void SchematicCanvas::fit() {
    if (state_->diagram)
        view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
}
void SchematicCanvas::zoomIn() { static_cast<NavigationView*>(view_)->zoom(1.2); }
void SchematicCanvas::zoomOut() { static_cast<NavigationView*>(view_)->zoom(1 / 1.2); }
const LayoutDiagram* SchematicCanvas::diagram() const noexcept {
    return state_->diagram ? &*state_->diagram : nullptr;
}
QGraphicsScene* SchematicCanvas::scene() const noexcept { return scene_; }
QGraphicsView* SchematicCanvas::view() const noexcept { return view_; }
QString SchematicCanvas::inspection() const { return inspector_->toPlainText(); }
QString SchematicCanvas::status() const { return status_->text(); }
std::optional<LabelOwner> SchematicCanvas::selection() const { return state_->selection; }
void SchematicCanvas::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange && state_->graph) {
        const auto graph = *state_->graph;
        const auto selected = state_->selection;
        if (setGraph(graph) && selected)
            select(*selected);
    }
}
} // namespace openece::schematic::qt
