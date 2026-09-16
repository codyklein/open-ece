#pragma once
#include <QMainWindow>

namespace openece::gui {
class MainWindow final : public QMainWindow {
  public:
    explicit MainWindow(QWidget* parent = nullptr);
};
} // namespace openece::gui
