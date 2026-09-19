#pragma once
#include <openece/communications/ber.hpp>
#include <openece/communications/link.hpp>
#include <qwt_plot.h>
class QwtPlotRescaler;
namespace openece::gui {
inline constexpr std::size_t communications_plot_points = 2048;
class CommunicationsPlot final : public QwtPlot {
  public:
    explicit CommunicationsPlot(QWidget* parent = nullptr);
    void clear();
    void waveform(const communications::LinkResult&, bool imaginary);
    void constellation(const communications::LinkResult&);
    void ber(std::span<const communications::BerPoint>, communications::Modulation);

  private:
    QwtPlotRescaler* rescaler_ = nullptr; // QObject child of the canvas.
};
} // namespace openece::gui
