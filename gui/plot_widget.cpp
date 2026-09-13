#include "plot_widget.hpp"

#include <qwt_legend.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_zoomer.h>
#include <qwt_symbol.h>
#include <qwt_text.h>

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
    if (comparison_) {
        comparison_->setSamples(QVector<double>{}, QVector<double>{});
        comparison_->setVisible(false);
        if (legend())
            legend()->hide();
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

void PlotWidget::set_comparison(std::span<const double> x, std::span<const double> y,
                                std::span<const double> filtered_x,
                                std::span<const double> filtered_y, double x_max, double y_min,
                                double y_max) {
    if (filtered_x.size() != filtered_y.size() ||
        filtered_x.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("Plot coordinates must have equal, representable lengths");
    }
    set_samples(x, y, x_max, y_min, y_max);
    if (!comparison_) {
        QwtText original_title("Original");
        original_title.setColor(curve_->pen().color());
        curve_->setTitle(original_title);
        curve_->setLegendAttribute(QwtPlotCurve::LegendShowLine);
        curve_->setLegendIconSize(QSize(24, 10));
        comparison_ = new QwtPlotCurve("Filtered");
        comparison_->setPen(QColor(25, 133, 91), 1.5);
        QwtText filtered_title("Filtered");
        filtered_title.setColor(comparison_->pen().color());
        comparison_->setTitle(filtered_title);
        comparison_->setLegendAttribute(QwtPlotCurve::LegendShowLine);
        comparison_->setLegendIconSize(QSize(24, 10));
        comparison_->setStyle(curve_->style());
        comparison_->setRenderHint(QwtPlotItem::RenderAntialiased);
        comparison_->attach(this);
        insertLegend(new QwtLegend, QwtPlot::BottomLegend);
    }
    comparison_->setSamples(filtered_x.data(), filtered_y.data(),
                            static_cast<int>(filtered_x.size()));
    comparison_->setVisible(true);
    legend()->show();
    replot();
    zoomer_->setZoomBase();
}

void PlotWidget::clear() {
    curve_->setSamples(QVector<double>{}, QVector<double>{});
    if (comparison_) {
        comparison_->setSamples(QVector<double>{}, QVector<double>{});
        comparison_->setVisible(false);
        legend()->hide();
    }
    replot();
}

} // namespace openece::gui
