#pragma once

#include <QMainWindow>

class QDoubleSpinBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace openece::gui {

class PlotWidget;

class MainWindow final : public QMainWindow {
  public:
    explicit MainWindow(QWidget* parent = nullptr);

  private:
    void generate();
    void change_phase_unit();

    // All widgets are owned by the QObject parent tree; these are observing pointers.
    QDoubleSpinBox* amplitude_;
    QDoubleSpinBox* frequency_;
    QLineEdit* phase_;
    QPushButton* pi_button_;
    QComboBox* phase_unit_;
    QComboBox* window_;
    bool phase_in_radians_ = false;
    QDoubleSpinBox* sample_rate_;
    QDoubleSpinBox* duration_;
    QLabel* status_;
    QLabel* summary_;
    PlotWidget* time_plot_;
    PlotWidget* spectrum_plot_;
};

} // namespace openece::gui
