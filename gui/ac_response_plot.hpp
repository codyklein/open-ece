#pragma once
#include <optional>
#include <qwt_plot.h>
#include <span>
namespace openece::gui {
class AcResponsePlot final : public QwtPlot {
  public:
    explicit AcResponsePlot(bool phase, QWidget* parent = nullptr);
    void set_response(std::span<const double> frequencies,
                      std::span<const std::optional<double>> values, bool logarithmic,
                      const QString& label);
    void clear();

  private:
    bool phase_;
};
} // namespace openece::gui
