#pragma once
#include "transient_execution.hpp"
#include <QAbstractTableModel>
#include <qwt_plot.h>
class QwtPlotCurve;
namespace openece::gui {
inline constexpr std::size_t transient_plot_buckets = 1024;
// Min/max envelope plus every breakpoint side, in original accepted sample order.
std::vector<std::size_t> transient_plot_indices(const transient_core::Result&,
                                                const std::vector<double>&);
class TransientTraceModel final : public QAbstractTableModel {
  public:
    explicit TransientTraceModel(QObject* parent) : QAbstractTableModel(parent) {}
    void set_result(std::shared_ptr<const transient_core::Result>, std::vector<TransientProbeLabel>,
                    QString time_unit, double time_scale);
    int rowCount(const QModelIndex& = {}) const override;
    int columnCount(const QModelIndex& = {}) const override;
    QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;
    QVariant headerData(int, Qt::Orientation, int role = Qt::DisplayRole) const override;

  private:
    std::shared_ptr<const transient_core::Result> result_;
    std::vector<TransientProbeLabel> probes_;
    QString time_unit_ = "s";
    double time_scale_ = 1;
};
class TransientPlot final : public QwtPlot {
  public:
    explicit TransientPlot(QWidget* parent);
    QSize sizeHint() const override { return minimumSize(); }
    void clear();
    void show_trace(const transient_core::Result&, const TransientProbeLabel&,
                    const QString& time_unit, double time_scale);

  private:
    QwtPlotCurve* curve_;
};
} // namespace openece::gui
