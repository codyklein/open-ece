#include "plot_widget.hpp"

#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_zoomer.h>
#include <qwt_symbol.h>

#include <QColor>
#include <QPen>
#include <QVector>

#include <limits>
#include <stdexcept>

namespace openece::gui {

PlotWidget::PlotWidget(const QString& title, const QString& x_label, const QString& y_label,
                       bool spectrum, QWidget* parent)
    : QwtPlot(parent), curve_(new QwtPlotCurve), zoomer_(new QwtPlotZoomer(canvas())) {
    setTitle(title);
    setAxisTitle(QwtPlot::xBottom, x_label);
    setAxisTitle(QwtPlot::yLeft, y_label);
    setCanvasBackground(Qt::white);
    setMinimumSize(400, 240);
    auto* grid = new QwtPlotGrid;
    grid->setMajorPen(QPen(QColor(205, 212, 221), 0.0, Qt::DotLine));
    grid->attach(this);
    curve_->setPen(spectrum ? QColor(161, 78, 17) : QColor(22, 96, 164), 1.5);
    curve_->setStyle(spectrum ? QwtPlotCurve::Sticks : QwtPlotCurve::Lines);
    curve_->setRenderHint(QwtPlotItem::RenderAntialiased);
    curve_->attach(this);
    zoomer_->setRubberBandPen(QPen(QColor(22, 96, 164)));
    zoomer_->setTrackerMode(QwtPicker::AlwaysOff);
    setToolTip("Drag a rectangle to zoom. Right-click to zoom out; Ctrl+right-click resets.");
}

void PlotWidget::set_samples(std::span<const double> x, std::span<const double> y, double x_max,
                             double y_min, double y_max) {
    if (x.size() != y.size() ||
        x.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("Plot coordinates must have equal, representable lengths");
    }
    // Qwt copies the arrays: plots never borrow storage from temporary analysis results.
    curve_->setSamples(x.data(), y.data(), static_cast<int>(x.size()));
    // A singleton cannot form a line segment, so show its sample explicitly.
    curve_->setSymbol(x.size() == 1
                          ? new QwtSymbol(QwtSymbol::Ellipse, QBrush(curve_->pen().color()),
                                          curve_->pen(), QSize(6, 6))
                          : nullptr);
    setAxisScale(QwtPlot::xBottom, 0.0, x_max);
    setAxisScale(QwtPlot::yLeft, y_min, y_max);
    replot();
    zoomer_->setZoomBase();
}

void PlotWidget::clear() {
    curve_->setSamples(QVector<double>{}, QVector<double>{});
    replot();
}

} // namespace openece::gui
