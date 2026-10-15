#include "canvas.hpp"
#include "preview_controls.hpp"
#include "schematic_preview_cases.hpp"
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QGraphicsView>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("OpenECE schematic developer preview");
    QWidget window;
    window.setWindowTitle(
        "OpenECE — read-only schematic developer preview (not a product workspace)");
    auto* layout = new QVBoxLayout(&window);
    auto* chooser = openece::schematic::qt::make_preview_fixture_chooser(&window);
    auto* label = new QLabel("&Review fixture:", &window);
    label->setBuddy(chooser);
    layout->addWidget(label);
    layout->addWidget(chooser);
    auto* canvas = new openece::schematic::qt::SchematicCanvas(&window);
    layout->addWidget(canvas, 1);
    const auto examples = openece::schematic::qt::preview_cases([](const std::string& name) {
        QFile file(":/schematic-preview/" + QString::fromStdString(name) + ".openece");
        if (!file.open(QIODevice::ReadOnly))
            throw std::runtime_error("Missing embedded preview example");
        return openece::project::decode_project(file.readAll().toStdString()).snapshot;
    });
    for (const auto& e : examples)
        chooser->addItem(e.name);
    QObject::connect(chooser, &QComboBox::currentIndexChanged, canvas,
                     [canvas, examples](int index) {
                         if (index >= 0)
                             canvas->setGraph(examples.at(static_cast<std::size_t>(index)).graph);
                     });
    window.resize(1280, 850);
    window.show();
    canvas->setGraph(examples.front().graph);
    if (app.arguments().contains("--smoke")) {
        for (std::size_t i = 0; i < examples.size(); ++i) {
            chooser->setCurrentIndex(static_cast<int>(i));
            if (!canvas->diagram() ||
                canvas->diagram()->components.size() != examples[i].graph.components().size())
                return 1;
            QApplication::processEvents();
            QImage image(800, 600, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            canvas->view()->render(&painter);
        }
        return 0;
    }
    return app.exec();
}
