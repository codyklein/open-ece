#pragma once
#include <QFont>
#include <QPainterPath>
#include <QString>
#include <schematic/layout.hpp>

namespace openece::schematic::qt {
struct DisplayLabel {
    LabelOwner owner;
    LabelRole role;
    QString text;
    Extent extent;
    bool operator==(const DisplayLabel&) const = default;
};
struct MeasuredLabels {
    LayoutMetrics metrics;
    std::vector<DisplayLabel> labels;
};
// One diagram unit maps to one Qt logical pixel before the view transform.
// Device-pixel ratio is handled by Qt, never multiplied into layout measurements.
MeasuredLabels measure_labels(const SchematicGraph&, const QFont&);
QString display_text(const std::string&, const QFont&);
struct SymbolGeometry {
    QPainterPath path;
    QPointF positive, negative;
    std::optional<QPointF> plus, minus, arrow_tail, arrow_tip;
};
SymbolGeometry symbol_geometry(const ComponentPlacement&);
// One path per original wire; gaps belong only to first_wire at explicit crossings.
std::vector<QPainterPath> wire_geometry(const LayoutDiagram&);
QPointF point(Point);
QRectF rectangle(Rectangle);
QString analysis_name(Analysis);
QString reference_text(const NodeReference&);
} // namespace openece::schematic::qt
