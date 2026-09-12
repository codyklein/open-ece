#pragma once

#include <qwt_plot.h>

#include <span>

class QwtPlotCurve;
class QwtPlotZoomer;

namespace openece::gui {

class PlotWidget final : public QwtPlot {
  public:
    PlotWidget(const QString& title, const QString& x_label, const QString& y_label, bool spectrum,
               QWidget* parent = nullptr);
    void set_samples(std::span<const double> x, std::span<const double> y, double x_max,
                     double y_min, double y_max);
    void clear();

  private:
    QwtPlotCurve* curve_;   // Owned by QwtPlot's auto-deleting item dictionary.
    QwtPlotZoomer* zoomer_; // QObject child of the canvas.
};

} // namespace openece::gui
