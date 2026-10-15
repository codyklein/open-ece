#pragma once
#include <QComboBox>

namespace openece::schematic::qt {
// Developer-preview control shared with its UI tests, not part of SchematicCanvas.
inline QComboBox* make_preview_fixture_chooser(QWidget* parent) {
    auto* chooser = new QComboBox(parent);
    chooser->setObjectName("schematicFixtureChooser");
    chooser->setAccessibleName("Review fixture");
    // Match the canvas panels without restyling text, arrow or popup subcontrols.
    // Both states reserve the same logical width, so focus cannot move the layout.
    chooser->setStyleSheet("QComboBox#schematicFixtureChooser { border: 3px solid #46505e; }"
                           "QComboBox#schematicFixtureChooser:focus { border-color: #96aef2; }");
    return chooser;
}
} // namespace openece::schematic::qt
