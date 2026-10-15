#pragma once
#include <QComboBox>
#include <QPainter>

namespace openece::schematic::qt {
// Preserve native combo subcontrols and their geometry; only its outer focus
// frame is painted here. This control belongs to the developer preview alone.
class PreviewFixtureChooser final : public QComboBox {
  public:
    explicit PreviewFixtureChooser(QWidget* parent) : QComboBox(parent) {}

  protected:
    void paintEvent(QPaintEvent* event) override {
        QComboBox::paintEvent(event);
        QPainter painter(this);
        painter.setPen(QPen(QColor(hasFocus() ? "#96aef2" : "#46505e"), 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect().adjusted(1, 1, -2, -2));
    }
};

// Developer-preview control shared with its UI tests, not part of SchematicCanvas.
inline QComboBox* make_preview_fixture_chooser(QWidget* parent) {
    auto* chooser = new PreviewFixtureChooser(parent);
    chooser->setObjectName("schematicFixtureChooser");
    chooser->setAccessibleName("Review fixture");
    return chooser;
}
} // namespace openece::schematic::qt
