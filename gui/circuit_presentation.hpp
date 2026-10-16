#pragma once
#include <QWidget>
#include <functional>
#include <optional>
#include <schematic/model.hpp>
class QComboBox;
class QStackedWidget;
class QTimer;
namespace openece::schematic::qt {
class SchematicCanvas;
}
namespace openece::gui {
class DraftView;
// Retains the actual editors. All schematic state is owned, derived and transient.
class CircuitPresentation final : public QWidget {
  public:
    using Project = std::function<schematic::SchematicGraph()>;
    CircuitPresentation(DraftView* owner, QWidget* tables, const QString& prefix, Project project,
                        QWidget* parent);
    ~CircuitPresentation() override;
    schematic::qt::SchematicCanvas* canvas() const { return canvas_; }

  protected:
    void showEvent(QShowEvent*) override;

  private:
    void schedule_refresh();
    void refresh();
    DraftView* owner_;
    Project project_;
    QComboBox* choice_;
    QStackedWidget* pages_;
    QTimer* refresh_;
    schematic::qt::SchematicCanvas* canvas_ = nullptr;
    std::optional<schematic::SchematicGraph> graph_;
};
} // namespace openece::gui
