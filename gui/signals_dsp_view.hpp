#pragma once

#include <QWidget>

class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace openece::gui {

class PlotWidget;

class SignalsDspView final : public QWidget {
  public:
    explicit SignalsDspView(QWidget* parent = nullptr);

  private:
    void generate();
    void change_phase_unit();
    void clear_results();

    // All widgets are owned by the QObject parent tree; these are observing pointers.
    QDoubleSpinBox* amplitude_;
    QDoubleSpinBox* frequency_;
    QLineEdit* phase_;
    QPushButton* pi_button_;
    QComboBox* phase_unit_;
    QComboBox* window_;
    QComboBox* filter_;
    QSpinBox* tap_count_;
    QDoubleSpinBox* cutoff_;
    QLabel* response_summary_;
    PlotWidget* response_plot_;
    bool phase_in_radians_ = false;
    QDoubleSpinBox* sample_rate_;
    QDoubleSpinBox* duration_;
    QLabel* status_;
    QLabel* summary_;
    PlotWidget* time_plot_;
    PlotWidget* spectrum_plot_;
};

} // namespace openece::gui
