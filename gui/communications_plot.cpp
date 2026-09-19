#include "communications_plot.hpp"
#include <QVector>
#include <algorithm>
#include <cmath>
#include <qwt_interval.h>
#include <qwt_legend.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_rescaler.h>
#include <qwt_scale_engine.h>
#include <qwt_symbol.h>
#include <qwt_text.h>
namespace openece::gui {
namespace {
void curve(QwtPlot* plot, const QString& title, const QVector<double>& x, const QVector<double>& y,
           QColor color, QwtPlotCurve::CurveStyle style,
           QwtSymbol::Style symbol = QwtSymbol::NoSymbol) {
    auto* item = new QwtPlotCurve(title);
    item->setSamples(x, y);
    item->setPen(color, 1.3);
    item->setStyle(style);
    item->setLegendAttribute(style == QwtPlotCurve::NoCurve ? QwtPlotCurve::LegendShowSymbol
                                                            : QwtPlotCurve::LegendShowLine);
    if (symbol != QwtSymbol::NoSymbol)
        item->setSymbol(new QwtSymbol(symbol, QBrush(color), QPen(color), QSize(6, 6)));
    item->attach(plot);
}
} // namespace
CommunicationsPlot::CommunicationsPlot(QWidget* parent) : QwtPlot(parent) {
    setMinimumSize(350, 210);
    setCanvasBackground(Qt::white);
    insertLegend(new QwtLegend, QwtPlot::BottomLegend);
    auto* grid = new QwtPlotGrid;
    grid->setMajorPen(QPen(QColor(210, 210, 210), 0, Qt::DotLine));
    grid->attach(this);
}
void CommunicationsPlot::clear() {
    if (rescaler_)
        rescaler_->setEnabled(false);
    detachItems(QwtPlotItem::Rtti_PlotCurve, true);
    detachItems(QwtPlotItem::Rtti_PlotMarker, true);
    replot();
}
void CommunicationsPlot::waveform(const communications::LinkResult& result, bool imaginary) {
    clear();
    setTitle(imaginary ? "Q waveform" : "I waveform");
    setAxisTitle(xBottom, "Time (s)");
    setAxisTitle(yLeft, "Normalized amplitude");
    setAxisScaleEngine(yLeft, new QwtLinearScaleEngine);
    setAxisAutoScale(xBottom);
    setAxisAutoScale(yLeft);
    QVector<double> x, tx, rx;
    const auto count = std::min(communications_plot_points, result.transmitted.size());
    for (std::size_t i = 0; i < count; ++i) {
        x.push_back(result.transmitted.time_seconds(i));
        auto a = result.transmitted.samples()[i], b = result.received.samples()[i];
        tx.push_back(imaginary ? a.imag() : a.real());
        rx.push_back(imaginary ? b.imag() : b.real());
    }
    curve(this, "Received", x, rx, QColor(190, 110, 25), QwtPlotCurve::Lines,
          count == 1 ? QwtSymbol::Ellipse : QwtSymbol::NoSymbol);
    curve(this, "Transmitted", x, tx, QColor(20, 90, 170), QwtPlotCurve::Steps,
          count == 1 ? QwtSymbol::Ellipse : QwtSymbol::NoSymbol);
    replot();
}
void CommunicationsPlot::constellation(const communications::LinkResult& result) {
    clear();
    setTitle("Matched-filter decision samples");
    setAxisTitle(xBottom, "I");
    setAxisTitle(yLeft, "Q");
    setAxisScaleEngine(yLeft, new QwtLinearScaleEngine);
    QVector<double> x, y;
    double extent = 1.5;
    for (std::size_t i = 0; i < std::min(communications_plot_points, result.decisions.size());
         ++i) {
        const auto s = result.decisions[i];
        x.push_back(s.real());
        y.push_back(s.imag());
        extent = std::max({extent, std::abs(s.real()) * 1.1, std::abs(s.imag()) * 1.1});
    }
    curve(this, "Received decisions", x, y, QColor(180, 110, 25), QwtPlotCurve::NoCurve,
          QwtSymbol::Ellipse);
    const std::vector<std::uint8_t> bits =
        result.frame.modulation == communications::Modulation::bpsk
            ? std::vector<std::uint8_t>{0, 1}
            : std::vector<std::uint8_t>{0, 0, 0, 1, 1, 1, 1, 0};
    const auto ideal =
        communications::map_bits(communications::BitSequence(bits), result.frame.modulation);
    x.clear();
    y.clear();
    const QStringList labels = result.frame.modulation == communications::Modulation::bpsk
                                   ? QStringList{"0", "1"}
                                   : QStringList{"00", "01", "11", "10"};
    for (std::size_t i = 0; i < ideal.size(); ++i) {
        x.push_back(ideal[i].real());
        y.push_back(ideal[i].imag());
        auto* marker = new QwtPlotMarker;
        marker->setValue(ideal[i].real(), ideal[i].imag());
        marker->setLabel(labels[static_cast<int>(i)]);
        marker->setLabelAlignment(Qt::AlignTop | Qt::AlignRight);
        marker->attach(this);
    }
    curve(this, "Ideal symbols", x, y, QColor(20, 90, 170), QwtPlotCurve::NoCurve,
          QwtSymbol::XCross);
    setAxisScale(xBottom, -extent, extent);
    setAxisScale(yLeft, -extent, extent);
    if (!rescaler_) {
        rescaler_ = new QwtPlotRescaler(canvas(), xBottom, QwtPlotRescaler::Fitting);
        rescaler_->setAspectRatio(yLeft, 1);
        rescaler_->setExpandingDirection(QwtPlotRescaler::ExpandBoth);
    }
    rescaler_->setIntervalHint(xBottom, QwtInterval(-extent, extent));
    rescaler_->setIntervalHint(yLeft, QwtInterval(-extent, extent));
    rescaler_->setEnabled(true);
    replot();
    rescaler_->rescale();
}
void CommunicationsPlot::ber(std::span<const communications::BerPoint> points,
                             communications::Modulation modulation) {
    clear();
    setTitle("BER: coherent BPSK / Gray QPSK");
    setAxisTitle(xBottom, "Eb/N0 (dB)");
    setAxisTitle(yLeft, "Bit error probability (log)");
    setAxisScaleEngine(yLeft, new QwtLogScaleEngine);
    setAxisAutoScale(xBottom);
    setAxisAutoScale(yLeft);
    QVector<double> x, theory, mx, my, ux, uy, px, py;
    for (const auto& p : points) {
        x.push_back(p.eb_n0_db);
        theory.push_back(communications::theoretical_ber(modulation, p.eb_n0_db));
        if (!p.bits_tested)
            continue;
        if (!p.bit_errors) {
            ux.push_back(p.eb_n0_db);
            uy.push_back(communications::zero_error_upper_bound95(p.bits_tested));
        } else if (p.complete()) {
            mx.push_back(p.eb_n0_db);
            my.push_back(*communications::measured_ber(p));
        } else {
            px.push_back(p.eb_n0_db);
            py.push_back(*communications::measured_ber(p));
        }
    }
    curve(this, "Theory", x, theory, QColor(20, 90, 170), QwtPlotCurve::Lines,
          x.size() == 1 ? QwtSymbol::XCross : QwtSymbol::NoSymbol);
    curve(this, "Measured (complete)", mx, my, QColor(20, 140, 70), QwtPlotCurve::NoCurve,
          QwtSymbol::Ellipse);
    curve(this, "Measured (partial)", px, py, QColor(150, 90, 170), QwtPlotCurve::NoCurve,
          QwtSymbol::Diamond);
    curve(this, "0 errors: 95% fixed-N upper bound", ux, uy, QColor(180, 110, 25),
          QwtPlotCurve::NoCurve, QwtSymbol::DTriangle);
    replot();
}
} // namespace openece::gui
