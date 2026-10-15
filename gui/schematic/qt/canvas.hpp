#pragma once
#include "render_geometry.hpp"
#include <QWidget>
#include <memory>
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QPlainTextEdit;
class QTreeWidget;
namespace openece::schematic::qt {
// Read-only owned presentation snapshot. No draft references, numerical objects or edit signals.
class SchematicCanvas final : public QWidget {
  public:
    explicit SchematicCanvas(QWidget* parent = nullptr);
    ~SchematicCanvas() override;
    bool setGraph(const SchematicGraph&);
    // External geometry is checked independently before replacing the old scene.
    // Metrics must come from measure_labels(graph, canvas.font()).
    bool setDiagram(const SchematicGraph&, const LayoutDiagram&, const LayoutMetrics&);
    void showFailure(const QString&);
    void fit();
    void zoomIn();
    void zoomOut();
    bool select(const LabelOwner&);
    const LayoutDiagram* diagram() const noexcept;
    QGraphicsScene* scene() const noexcept;
    QGraphicsView* view() const noexcept;
    QString inspection() const;
    QString status() const;
    std::optional<LabelOwner> selection() const;

  protected:
    void changeEvent(QEvent*) override;

  private:
    struct State;
    std::unique_ptr<State> state_;
    QGraphicsScene* scene_;
    QGraphicsView* view_;
    QLabel* status_;
    QPlainTextEdit* inspector_;
    QTreeWidget* entities_;
    void install(const SchematicGraph&, const LayoutDiagram&, const MeasuredLabels&);
};
} // namespace openece::schematic::qt
