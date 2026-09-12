#pragma once

#include <QMainWindow>

class QDoubleSpinBox;
class QLabel;

namespace openece::gui {

class PlotWidget;

class MainWindow final : public QMainWindow {
  public:
    explicit MainWindow(QWidget* parent = nullptr);

  private:
    void generate();

    // All widgets are owned by the QObject parent tree; these are observing pointers.
    QDoubleSpinBox* amplitude_;
    QDoubleSpinBox* frequency_;
    QDoubleSpinBox* phase_;
    QDoubleSpinBox* sample_rate_;
    QDoubleSpinBox* duration_;
    QLabel* status_;
    QLabel* summary_;
    PlotWidget* time_plot_;
    PlotWidget* spectrum_plot_;
};

} // namespace openece::gui
