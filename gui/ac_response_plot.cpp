#include "ac_response_plot.hpp"
#include <QPen>
#include <QVector>
#include <cmath>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_scale_engine.h>
#include <qwt_symbol.h>
#include <stdexcept>
namespace openece::gui {
AcResponsePlot::AcResponsePlot(bool phase, QWidget* parent) : QwtPlot(parent), phase_(phase) {
    setTitle(phase ? "Wrapped phase" : "Magnitude response");
    setAxisTitle(xBottom, "Frequency (Hz)");
    setMinimumSize(350, 210);
    setCanvasBackground(Qt::white);
    auto* grid = new QwtPlotGrid;
    grid->setMajorPen(QPen(QColor(205, 212, 221), 0, Qt::DotLine));
    grid->attach(this);
}
void AcResponsePlot::clear() {
    detachItems(QwtPlotItem::Rtti_PlotCurve, true);
    replot();
}
void AcResponsePlot::set_response(std::span<const double> frequencies,
                                  std::span<const std::optional<double>> values, bool logarithmic,
                                  const QString& label) {
    if (frequencies.size() != values.size())
        throw std::invalid_argument("Response plot size mismatch");
    detachItems(QwtPlotItem::Rtti_PlotCurve, true);
    setAxisTitle(yLeft, label);
    setAxisScaleEngine(xBottom, logarithmic ? static_cast<QwtScaleEngine*>(new QwtLogScaleEngine)
                                            : new QwtLinearScaleEngine);
    if (frequencies.size() > 1)
        setAxisScale(xBottom, frequencies.front(), frequencies.back());
    if (phase_)
        setAxisScale(yLeft, -180, 180);
    else
        setAxisAutoScale(yLeft);
    QVector<double> x, y;
    auto flush = [&] {
        if (x.empty())
            return;
        auto* curve = new QwtPlotCurve;
        curve->setPen(QColor(22, 96, 164), 1.5);
        curve->setRenderHint(QwtPlotItem::RenderAntialiased);
        // Each uninterrupted run is a separate owned curve: never bridge missing values.
        curve->setSamples(x, y);
        if (x.size() == 1)
            curve->setSymbol(new QwtSymbol(QwtSymbol::Ellipse, QBrush(curve->pen().color()),
                                           curve->pen(), QSize(5, 5)));
        curve->attach(this);
        x.clear();
        y.clear();
    };
    for (std::size_t i = 0; i < frequencies.size(); ++i) {
        if (!values[i] || !std::isfinite(*values[i])) {
            flush();
            continue;
        }
        if (phase_ && !y.empty() && std::abs(*values[i] - y.back()) > 180)
            flush();
        x.push_back(frequencies[i]);
        y.push_back(*values[i]);
    }
    flush();
    replot();
}
} // namespace openece::gui
