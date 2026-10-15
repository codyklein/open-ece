#include "render_geometry.hpp"
#include <QFontMetricsF>
#include <QTransform>
#include <algorithm>
#include <cmath>

namespace openece::schematic::qt {
QPointF point(Point p) { return {static_cast<qreal>(p.x), static_cast<qreal>(p.y)}; }
QRectF rectangle(Rectangle r) {
    return {point(r.origin),
            QSizeF(static_cast<qreal>(r.size.width), static_cast<qreal>(r.size.height))};
}
QString analysis_name(Analysis a) {
    return a == Analysis::dc ? "DC" : a == Analysis::ac ? "AC" : "Transient";
}
QString reference_text(const NodeReference& ref) {
    if (const auto* n = std::get_if<DeclaredNode>(&ref))
        return QString("node [%1]").arg(n->key.value);
    if (const auto* n = std::get_if<DanglingNode>(&ref))
        return QString("missing node [%1] (dangling)").arg(n->key.value);
    return "unselected (open)";
}
QString display_text(const std::string& raw, const QFont& font) {
    QString text = QString::fromStdString(raw);
    // Single-line display only: make controls visible, retain exact originals in inspection.
    text.replace('\r', QString::fromUtf8("␍"));
    text.replace('\n', QString::fromUtf8("↵"));
    text.replace('\t', QString::fromUtf8("⇥"));
    text.replace(QChar(0x2028), QString::fromUtf8("↵"));
    text.replace(QChar(0x2029), QString::fromUtf8("↵"));
    for (qsizetype i = 0; i < text.size(); ++i)
        if (text[i].unicode() < 32 || text[i].unicode() == 127)
            text[i] = QChar(0xfffd);
    const QFontMetricsF fm(font);
    return fm.elidedText(text, Qt::ElideRight, std::max(260.0, 22.0 * fm.height()));
}
MeasuredLabels measure_labels(const SchematicGraph& graph, const QFont& font) {
    MeasuredLabels out;
    const QFontMetricsF fm(font);
    auto add = [&](LabelOwner owner, LabelRole role, const std::string& raw) {
        auto text = display_text(raw, font);
        // Include ink bearings and advance; 4 units per side protect antialiasing overhang.
        const auto ink = fm.boundingRect(text);
        const auto width = std::max({fm.horizontalAdvance(text), ink.width(), ink.right()}) +
                           std::max(0.0, -ink.left()) + 8.0;
        const Extent extent{
            static_cast<Coordinate>(std::ceil(std::max(1.0, width))),
            static_cast<Coordinate>(std::ceil(std::max(fm.height(), ink.height()) + 8.0))};
        out.metrics.labels.push_back({owner, role, extent});
        out.labels.push_back({owner, role, std::move(text), extent});
    };
    for (const auto& n : graph.nodes())
        add(n.key, LabelRole::node_name, node_label_text(n));
    for (const auto& c : graph.components())
        for (auto role :
             {LabelRole::component_name, LabelRole::component_value, LabelRole::component_detail})
            add(c.key, role, component_label_text(c, role));
    return out;
}
SymbolGeometry symbol_geometry(const ComponentPlacement& c) {
    SymbolGeometry out;
    const qreal length = QLineF(point(c.positive), point(c.negative)).length();
    const qreal half = length / 2.0;
    const qreal width = static_cast<qreal>(
        (c.direction == Direction::down || c.direction == Direction::up) ? c.body.size.width
                                                                         : c.body.size.height);
    const qreal radius = width / 2.0;
    auto line = [&](qreal x1, qreal y1, qreal x2, qreal y2) {
        out.path.moveTo(x1, y1);
        out.path.lineTo(x2, y2);
    };
    line(-half, 0, -radius, 0);
    line(radius, 0, half, 0);
    switch (c.symbol) {
    case SymbolKind::resistor:
    case SymbolKind::generic:
        out.path.addRect(QRectF(-radius, -radius * 0.65, width, radius * 1.3));
        if (c.symbol == SymbolKind::generic) {
            line(-radius * .4, -radius * .4, radius * .4, radius * .4);
            line(-radius * .4, radius * .4, radius * .4, -radius * .4);
        }
        break;
    case SymbolKind::capacitor:
        line(-radius, 0, -radius * .28, 0);
        line(radius * .28, 0, radius, 0);
        line(-radius * .28, -radius, -radius * .28, radius);
        line(radius * .28, -radius, radius * .28, radius);
        break;
    case SymbolKind::inductor:
        out.path.moveTo(-radius, 0);
        for (int i = 0; i < 4; ++i) {
            const qreal x = -radius + static_cast<qreal>(i) * width / 4;
            out.path.cubicTo(x, -radius, x + width / 4, -radius, x + width / 4, 0);
        }
        break;
    case SymbolKind::voltage_source:
    case SymbolKind::current_source:
        out.path.addEllipse(QPointF(0, 0), radius, radius);
        if (c.symbol == SymbolKind::voltage_source) {
            out.plus = QPointF(-radius * .45, 0);
            out.minus = QPointF(radius * .45, 0);
        } else {
            out.arrow_tail = QPointF(-radius * .55, 0);
            out.arrow_tip = QPointF(radius * .55, 0);
            line(out.arrow_tail->x(), 0, out.arrow_tip->x(), 0);
            line(out.arrow_tip->x(), 0, radius * .15, -radius * .3);
            line(out.arrow_tip->x(), 0, radius * .15, radius * .3);
        }
        break;
    }
    QTransform transform;
    transform.translate(static_cast<qreal>(c.center.x), static_cast<qreal>(c.center.y));
    switch (c.direction) {
    case Direction::down:
        transform.rotate(90);
        break;
    case Direction::left:
        transform.rotate(180);
        break;
    case Direction::up:
        transform.rotate(270);
        break;
    default:
        break;
    }
    out.path = transform.map(out.path);
    out.positive = transform.map(QPointF(-half, 0));
    out.negative = transform.map(QPointF(half, 0));
    for (auto* p : {&out.plus, &out.minus, &out.arrow_tail, &out.arrow_tip})
        if (*p)
            *p = transform.map(**p);
    if (out.plus && out.minus) {
        const auto a = radius * .18;
        out.path.moveTo(*out.plus + QPointF(-a, 0));
        out.path.lineTo(*out.plus + QPointF(a, 0));
        out.path.moveTo(*out.plus + QPointF(0, -a));
        out.path.lineTo(*out.plus + QPointF(0, a));
        out.path.moveTo(*out.minus + QPointF(-a, 0));
        out.path.lineTo(*out.minus + QPointF(a, 0));
    }
    return out;
}
std::vector<QPainterPath> wire_geometry(const LayoutDiagram& d) {
    std::vector<std::vector<Point>> cuts(d.wires.size());
    for (const auto& c : d.crossings)
        cuts.at(c.first_wire).push_back(c.at);
    std::vector<QPainterPath> paths(d.wires.size());
    const auto gap = static_cast<qreal>(d.marker_radius) + 1.0;
    for (std::size_t w = 0; w < d.wires.size(); ++w) {
        const auto& wire = d.wires[w];
        for (std::size_t i = 1; i < wire.points.size(); ++i) {
            const auto a = point(wire.points[i - 1]), b = point(wire.points[i]);
            const QLineF line(a, b);
            const auto length = line.length();
            const auto unit = (b - a) / length;
            std::vector<qreal> distances;
            for (const auto cut : cuts[w]) {
                const auto p = point(cut);
                if ((a.x() == b.x() && p.x() == a.x() && p.y() > std::min(a.y(), b.y()) &&
                     p.y() < std::max(a.y(), b.y())) ||
                    (a.y() == b.y() && p.y() == a.y() && p.x() > std::min(a.x(), b.x()) &&
                     p.x() < std::max(a.x(), b.x())))
                    distances.push_back(QLineF(a, p).length());
            }
            std::sort(distances.begin(), distances.end());
            qreal start = 0;
            for (auto distance : distances) {
                const auto end = std::max(0.0, distance - gap);
                if (end > start) {
                    paths[w].moveTo(a + unit * start);
                    paths[w].lineTo(a + unit * end);
                }
                start = std::max(start, std::min(length, distance + gap));
            }
            if (start < length) {
                paths[w].moveTo(a + unit * start);
                paths[w].lineTo(b);
            }
        }
    }
    return paths;
}
} // namespace openece::schematic::qt
