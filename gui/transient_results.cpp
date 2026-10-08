#include "transient_results.hpp"
#include <QPen>
#include <algorithm>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_text.h>
namespace openece::gui {
std::vector<std::size_t> transient_plot_indices(const transient_core::Result& r,
                                                const std::vector<double>& values) {
    const auto n = r.times.size();
    std::vector<bool> selected(n, false);
    if (!n)
        return {};
    selected[0] = selected[n - 1] = true;
    const auto stride =
        std::max<std::size_t>(1, (n + transient_plot_buckets - 1) / transient_plot_buckets);
    for (std::size_t first = 0; first < n; first += stride) {
        const auto last = std::min(n, first + stride);
        auto min = first, max = first;
        selected[first] = selected[last - 1] = true;
        for (auto i = first; i < last; ++i) {
            if (r.times[i].side != transient_core::SampleSide::regular)
                selected[i] = true;
            if (values[i] < values[min])
                min = i;
            if (values[i] > values[max])
                max = i;
        }
        selected[min] = selected[max] = true;
    }
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < n; ++i)
        if (selected[i])
            indices.push_back(i);
    return indices;
}
void TransientTraceModel::set_result(std::shared_ptr<const transient_core::Result> r,
                                     std::vector<TransientProbeLabel> p, QString unit,
                                     double scale) {
    beginResetModel();
    result_ = std::move(r);
    probes_ = std::move(p);
    time_unit_ = std::move(unit);
    time_scale_ = scale;
    endResetModel();
}
int TransientTraceModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() || !result_ ? 0 : static_cast<int>(result_->times.size());
}
int TransientTraceModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(probes_.size()) + 2;
}
QVariant TransientTraceModel::data(const QModelIndex& i, int role) const {
    if (!result_ || !i.isValid() || i.row() < 0 || i.row() >= rowCount() || i.column() < 0 ||
        i.column() >= columnCount() || role != Qt::DisplayRole)
        return {};
    const auto row = static_cast<std::size_t>(i.row());
    if (i.column() == 0)
        return QString::number(result_->times[row].seconds / time_scale_, 'g', 17);
    if (i.column() == 1) {
        switch (result_->times[row].side) {
        case transient_core::SampleSide::regular:
            return "regular";
        case transient_core::SampleSide::before_breakpoint:
            return "before breakpoint";
        case transient_core::SampleSide::after_breakpoint:
            return "after breakpoint";
        }
    }
    const auto& p = probes_[static_cast<std::size_t>(i.column() - 2)];
    const auto& trace = p.voltage ? result_->voltages[p.index] : result_->currents[p.index];
    return QString::number(trace[row], 'g', 17);
}
QVariant TransientTraceModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole)
        return {};
    if (orientation == Qt::Vertical)
        return section + 1;
    if (section == 0)
        return "Time (" + time_unit_ + ")";
    if (section == 1)
        return "Sample side";
    if (section >= 2 && section < columnCount())
        return probes_[static_cast<std::size_t>(section - 2)].label;
    return {};
}
TransientPlot::TransientPlot(QWidget* parent) : QwtPlot(parent), curve_(new QwtPlotCurve) {
    setMinimumSize(350, 210);
    setCanvasBackground(Qt::white);
    auto* grid = new QwtPlotGrid;
    grid->setMajorPen(QPen(Qt::lightGray, 0, Qt::DotLine));
    grid->attach(this);
    curve_->setPen(QColor(40, 84, 197), 1.5);
    curve_->attach(this);
}
void TransientPlot::clear() {
    curve_->setSamples(QVector<QPointF>{});
    setTitle("No accepted trace");
    replot();
}
void TransientPlot::show_trace(const transient_core::Result& r, const TransientProbeLabel& p,
                               const QString& unit, double scale) {
    // Never let user names be interpreted as Qwt rich text.
    setTitle(QwtText(p.label, QwtText::PlainText));
    setAxisTitle(xBottom, "Time (" + unit + ")");
    setAxisTitle(yLeft,
                 p.voltage ? "Instantaneous voltage (V)" : "Current, positive → negative (A)");
    const auto& values = p.voltage ? r.voltages[p.index] : r.currents[p.index];
    QVector<QPointF> points;
    for (auto i : transient_plot_indices(r, values))
        points.push_back({r.times[i].seconds / scale, values[i]});
    curve_->setSamples(points);
    setAxisAutoScale(xBottom);
    setAxisAutoScale(yLeft);
    replot();
}
} // namespace openece::gui
