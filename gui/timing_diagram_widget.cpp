#include "timing_diagram_widget.hpp"
#include <QPen>
#include <algorithm>
#include <cmath>
#include <qwt_plot_curve.h>
#include <qwt_scale_draw.h>
#include <qwt_text.h>
#include <utility>

namespace openece::gui {
namespace {
class LaneScale final : public QwtScaleDraw {
  public:
    explicit LaneScale(QStringList labels) : labels_(std::move(labels)) {}
    QwtText label(double value) const override {
        const int index = static_cast<int>(labels_.size()) - 1 -
                          static_cast<int>(std::llround((value - 0.35) / 2.0));
        return index >= 0 && index < labels_.size() ? QwtText(labels_[index]) : QwtText();
    }

  private:
    QStringList labels_;
};
} // namespace
TimingDiagramWidget::TimingDiagramWidget(QWidget* parent) : QwtPlot(parent) {
    setObjectName("timing_diagram");
    setMinimumHeight(230);
    setCanvasBackground(Qt::white);
    setTitle("Visible node transitions — low = 0, high = 1");
    clear();
}
void TimingDiagramWidget::clear() {
    detachItems(QwtPlotItem::Rtti_PlotCurve, true);
    setAxisScaleDraw(QwtPlot::yLeft, new LaneScale({}));
    setAxisScale(QwtPlot::xBottom, 0, 1);
    setAxisScale(QwtPlot::yLeft, -0.5, 1.5);
    replot();
}
void TimingDiagramWidget::present(const digital::timing::SimulationSnapshot& snapshot,
                                  const QStringList& labels, double ticks_per_unit,
                                  const QString& unit) {
    detachItems(QwtPlotItem::Rtti_PlotCurve, true);
    setAxisScaleDraw(QwtPlot::yLeft, new LaneScale(labels));
    QList<double> major;
    const auto count = snapshot.traces.size();
    for (std::size_t i = 0; i < count; ++i) {
        const auto& trace = snapshot.traces[i];
        const double base = static_cast<double>(count - 1 - i) * 2.0;
        major.push_back(base + 0.35);
        QVector<QPointF> points;
        double previous = base;
        for (const auto& transition : trace.transitions) {
            const auto x = static_cast<double>(transition.at.ticks) / ticks_per_unit;
            const double y = base + (transition.value == digital::LogicValue::one ? 0.7 : 0.0);
            if (!points.empty())
                points.push_back({x, previous});
            points.push_back({x, y});
            previous = y;
        }
        if (!points.empty())
            points.push_back(
                {static_cast<double>(snapshot.reached.ticks) / ticks_per_unit, previous});
        auto* curve = new QwtPlotCurve(
            i < static_cast<std::size_t>(labels.size()) ? labels[static_cast<int>(i)] : QString());
        curve->setPen(QPen(QColor::fromHsv(static_cast<int>((i * 67) % 360), 190, 150), 2));
        curve->setSamples(points); // Qwt owns a copy; no pointers into a simulation session.
        curve->attach(this);
    }
    setAxisScaleDiv(QwtPlot::yLeft,
                    QwtScaleDiv(-0.5, static_cast<double>(count) * 2.0, {}, {}, major));
    setAxisTitle(QwtPlot::xBottom, "Simulation time (" + unit + ")");
    setAxisScale(QwtPlot::xBottom, 0,
                 static_cast<double>(std::max<std::uint64_t>(1, snapshot.reached.ticks)) /
                     ticks_per_unit);
    replot();
}
} // namespace openece::gui
