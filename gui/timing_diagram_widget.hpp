#pragma once
#include <openece/digital/timing_trace.hpp>
#include <qwt_plot.h>

namespace openece::gui {
class TimingDiagramWidget final : public QwtPlot {
  public:
    explicit TimingDiagramWidget(QWidget* parent = nullptr);
    void clear();
    void present(const digital::timing::SimulationSnapshot& snapshot, const QStringList& labels,
                 double ticks_per_unit, const QString& unit);
};
} // namespace openece::gui
