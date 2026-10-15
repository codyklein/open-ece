#include "schematic_layout_cases.hpp"
#include "schematic_preview_cases.hpp"
#include <QApplication>
#include <QDir>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QImage>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTest>
#include <QTreeWidget>
#include <QWheelEvent>
#include <schematic/qt/canvas.hpp>
namespace s = openece::schematic;
namespace q = s::qt;
class SchematicGuiTest final : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void symbolsAndPolarity() {
        for (const auto direction :
             {s::Direction::right, s::Direction::down, s::Direction::left, s::Direction::up}) {
            const bool vertical = direction == s::Direction::down || direction == s::Direction::up;
            const bool reverse = direction == s::Direction::left || direction == s::Direction::up;
            s::Point a = vertical ? s::Point{100, 60} : s::Point{60, 100};
            s::Point b = vertical ? s::Point{100, 140} : s::Point{140, 100};
            if (reverse)
                std::swap(a, b);
            for (const auto kind : {s::SymbolKind::resistor, s::SymbolKind::capacitor,
                                    s::SymbolKind::inductor, s::SymbolKind::voltage_source,
                                    s::SymbolKind::current_source, s::SymbolKind::generic}) {
                s::ComponentPlacement c{};
                c.direction = direction;
                c.symbol = kind;
                c.center = {100, 100};
                c.positive = a;
                c.negative = b;
                c.body =
                    vertical ? s::Rectangle{{84, 60}, {32, 80}} : s::Rectangle{{60, 84}, {80, 32}};
                const auto geometry = q::symbol_geometry(c);
                QVERIFY(QLineF(geometry.positive, q::point(a)).length() < .001);
                QVERIFY(QLineF(geometry.negative, q::point(b)).length() < .001);
                QVERIFY(geometry.path.elementCount() > 5);
                QVERIFY(q::rectangle(c.body)
                            .adjusted(-1, -1, 1, 1)
                            .contains(geometry.path.boundingRect()));
                if (kind == s::SymbolKind::voltage_source) {
                    QVERIFY(geometry.plus && geometry.minus);
                    QVERIFY(QLineF(*geometry.plus, q::point(a)).length() <
                            QLineF(*geometry.minus, q::point(a)).length());
                }
                if (kind == s::SymbolKind::current_source) {
                    QVERIFY(geometry.arrow_tip && geometry.arrow_tail);
                    QVERIFY(QLineF(*geometry.arrow_tip, q::point(b)).length() <
                            QLineF(*geometry.arrow_tail, q::point(b)).length());
                }
            }
        }
    }
    void measuredLabelsAndUnicode() {
        const auto examples = q::preview_cases();
        const auto& graph = examples[6].graph;
        const auto before = graph;
        q::SchematicCanvas canvas;
        canvas.resize(1200, 850);
        canvas.show();
        QVERIFY(canvas.setGraph(graph));
        QCOMPARE(graph, before);
        const auto measured = q::measure_labels(graph, canvas.font());
        const auto* d = canvas.diagram();
        QVERIFY(d);
        QVERIFY(s::verify_layout(graph, *d, measured.metrics).valid());
        bool elided = false;
        for (const auto& label : measured.labels) {
            if (label.text.contains(QChar(0x2026)))
                elided = true;
            const QFontMetricsF fm(canvas.font());
            QVERIFY(fm.horizontalAdvance(label.text) <= static_cast<qreal>(label.extent.width));
            QVERIFY(fm.height() <= static_cast<qreal>(label.extent.height));
        }
        QVERIFY(elided);
        const auto shown = q::display_text("a\n\t\r\xe2\x80\xa8", canvas.font());
        QVERIFY(!shown.contains('\n'));
        QVERIFY(!shown.contains(QChar(0x2028)));
        for (auto* item : canvas.scene()->items())
            if (auto* text = dynamic_cast<QGraphicsSimpleTextItem*>(item)) {
                bool fits = false;
                for (const auto& label : d->labels)
                    if (q::rectangle(label.bounds)
                            .adjusted(-.1, -.1, .1, .1)
                            .contains(text->sceneBoundingRect()))
                        fits = true;
                QVERIFY(fits);
            }
        QVERIFY(canvas.select(graph.components()[1].key));
        QVERIFY(canvas.inspection().contains("<b>literal markup</b> αβ Ω\n"));
        QVERIFY(canvas.inspection().contains(" 1e- "));
        QVERIFY(canvas.inspection().contains("missing node [999]"));
        auto font = canvas.font();
        font.setPointSizeF(font.pointSizeF() * 1.7);
        canvas.setFont(font);
        QVERIFY(canvas.diagram());
        QVERIFY(s::verify_layout(graph, *canvas.diagram(), q::measure_labels(graph, font).metrics)
                    .valid());
    }
    void visibleNonconnectingCrossings() {
        const auto graph = s::project_schematic(schematic_cases::wheatstone());
        const auto d = std::get<s::LayoutDiagram>(s::layout_schematic(graph));
        const auto paths = q::wire_geometry(d);
        QVERIFY(!d.crossings.empty());
        QPainterPathStroker stroke;
        stroke.setWidth(2);
        for (const auto& x : d.crossings) {
            QVERIFY(d.wires[x.first_wire].net != d.wires[x.second_wire].net);
            QVERIFY(!stroke.createStroke(paths[x.first_wire]).contains(q::point(x.at)));
            QVERIFY(stroke.createStroke(paths[x.second_wire]).contains(q::point(x.at)));
        }
        const auto x = d.crossings.front();
        QImage image(40, 40, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.translate(QPointF(20, 20) - q::point(x.at));
        painter.setPen(QPen(Qt::black, 2));
        painter.drawPath(paths[x.first_wire]);
        QCOMPARE(image.pixelColor(20, 20), QColor(Qt::white));
        painter.drawPath(paths[x.second_wire]);
        painter.end();
        QCOMPARE(image.pixelColor(20, 20), QColor(Qt::black));
    }
    void selectionGroundAndInspection() {
        q::SchematicCanvas canvas;
        const auto graph = s::project_schematic(schematic_cases::wheatstone());
        QVERIFY(canvas.setGraph(graph));
        const auto measuredCurrent = q::measure_labels(graph, canvas.font());
        QVERIFY(canvas.setDiagram(graph, *canvas.diagram(), measuredCurrent.metrics));
        const auto d = *canvas.diagram();
        QVERIFY(d.ground);
        QCOMPARE(d.ground->net.value, 0u);
        QVERIFY(canvas.select(s::NodeKey{s::Analysis::dc, 2}));
        QVERIFY(canvas.inspection().contains("Junctions"));
        QVERIFY(canvas.inspection().contains("no connection"));
        QVERIFY(!canvas.select(s::NodeKey{s::Analysis::ac, 2}));
        QCOMPARE(*canvas.selection(), s::LabelOwner(s::NodeKey{s::Analysis::dc, 2}));
        std::size_t highlighted = 0;
        for (auto* item : canvas.scene()->items())
            if (auto* path = dynamic_cast<QGraphicsPathItem*>(item);
                path && path->pen().color() == QColor("#96aef2"))
                ++highlighted;
        QVERIFY(highlighted > 0);
        QVERIFY(canvas.select(graph.components().front().key));
        QVERIFY(canvas.inspection().contains("positive → negative"));
        const auto measured = q::measure_labels(graph, canvas.font());
        auto corrupted = d;
        corrupted.wires.front().net = {s::Analysis::ac, 0};
        QVERIFY(!canvas.setDiagram(graph, corrupted, measured.metrics));
        QVERIFY(!canvas.diagram());
        QVERIFY(canvas.scene()->items().empty());
        QVERIFY(canvas.status().contains("unavailable"));
    }
    void markersAndIncomplete() {
        auto cases = q::preview_cases();
        q::SchematicCanvas canvas;
        QVERIFY(canvas.setGraph(cases[6].graph));
        const auto& d = *canvas.diagram();
        QVERIFY(!d.ground);
        std::size_t open = 0, missing = 0;
        for (const auto& terminal : d.terminals) {
            open += std::holds_alternative<s::Unselected>(terminal.reference);
            missing += std::holds_alternative<s::DanglingNode>(terminal.reference);
        }
        QVERIFY(open > 0 && missing > 0);
        QVERIFY(canvas.inspection().contains("Open circles"));
        QVERIFY(canvas.inspection().contains("Crosses: dangling"));
    }
    void navigationAndKeyboard() {
        q::SchematicCanvas canvas;
        canvas.resize(1100, 700);
        canvas.show();
        QVERIFY(canvas.setGraph(s::project_schematic(schematic_cases::divider())));
        QApplication::processEvents();
        canvas.fit();
        const auto scale = canvas.view()->transform().m11();
        const auto* diagram = canvas.diagram();
        const auto component = diagram->components.front();
        QTest::mouseClick(canvas.view()->viewport(), Qt::LeftButton, Qt::NoModifier,
                          canvas.view()->mapFromScene(q::point(component.center)));
        QCOMPARE(canvas.selection(), std::optional<s::LabelOwner>{component.source.key});
        const auto net = diagram->nets.front();
        QTest::mouseClick(canvas.view()->viewport(), Qt::LeftButton, Qt::NoModifier,
                          canvas.view()->mapFromScene(q::point(net.anchor)));
        QCOMPARE(canvas.selection(), std::optional<s::LabelOwner>{net.source.key});

        canvas.zoomIn();
        QVERIFY(canvas.view()->transform().m11() > scale);
        canvas.zoomOut();
        QVERIFY(std::abs(canvas.view()->transform().m11() - scale) < .001);
        auto* tree = canvas.findChild<QTreeWidget*>();
        QVERIFY(tree);
        tree->setFocus();
        tree->setCurrentItem(tree->topLevelItem(0)->child(0));
        QVERIFY(canvas.selection());
        QTest::keyClick(tree, Qt::Key_Down);
        QVERIFY(canvas.selection());
        const auto previous = canvas.selection();
        canvas.view()->setFocus();
        QTest::keyClick(canvas.view(), Qt::Key_Plus);
        QVERIFY(canvas.view()->transform().m11() > scale);
        QTest::keyClick(canvas.view(), Qt::Key_F);
        QVERIFY(std::abs(canvas.view()->transform().m11() - scale) < .01);
        canvas.zoomIn();
        canvas.zoomIn();
        canvas.zoomIn();
        auto* bar = canvas.view()->verticalScrollBar();
        bar->setValue(bar->maximum() / 2);
        const auto old = bar->value();
        QTest::keyClick(canvas.view(), Qt::Key_Down);
        QVERIFY(bar->value() >= old);
        QCOMPARE(canvas.view()->dragMode(), QGraphicsView::ScrollHandDrag);
        canvas.resize(640, 480);
        QApplication::processEvents();
        canvas.fit();
        QVERIFY(canvas.view()->viewport()->width() > 0);
        QCOMPARE(canvas.selection(), previous);
        const auto oldScale = canvas.view()->transform().m11();
        QWheelEvent wheel(QPointF(40, 40), QPointF(40, 40), {}, QPoint(0, 120), Qt::NoButton,
                          Qt::ControlModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(canvas.view()->viewport(), &wheel);
        QVERIFY(canvas.view()->transform().m11() > oldScale);
        for (auto* button : canvas.findChildren<QPushButton*>())
            QVERIFY(!button->accessibleName().isEmpty());
        QVERIFY(!canvas.findChild<QPlainTextEdit*>()->accessibleName().isEmpty());
    }
    void retainedSourceMetadata() {
        const auto graph = s::project_schematic(schematic_cases::large_sources());
        q::SchematicCanvas canvas;
        QVERIFY(canvas.setGraph(graph));
        QVERIFY(canvas.scene()->items().size() < 80);
        QVERIFY(canvas.select(graph.components().front().key));
        QVERIFY(canvas.inspection().contains(QString(500, 't')));
        QVERIFY(canvas.inspection().contains(QString(500, 'v')));
        QCOMPARE(canvas.diagram()->components.front().source.source->points.size(),
                 std::size_t(1024));
    }
    void developerScreenshots() {
        const auto directory = qEnvironmentVariable("OPENECE_SCHEMATIC_SCREENSHOTS");
        if (directory.isEmpty())
            QSKIP("Optional developer review images not requested");
        QDir().mkpath(directory);
        q::SchematicCanvas canvas;
        canvas.resize(1280, 850);
        canvas.show();
        const auto cases = q::preview_cases();
        for (std::size_t i = 0; i < cases.size(); ++i) {
            QVERIFY(canvas.setGraph(cases[i].graph));
            QApplication::processEvents();
            canvas.fit();
            QVERIFY(canvas.grab().save(directory + QString("/schematic-%1.png").arg(i)));
        }
    }
    void replacementFailureAndDensity() {
        q::SchematicCanvas canvas;
        canvas.resize(1200, 800);
        canvas.show();
        const auto cases = q::preview_cases();
        for (int pass = 0; pass < 2; ++pass)
            for (const auto& c : cases) {
                QVERIFY2(canvas.setGraph(c.graph),
                         qPrintable(c.name + ": " + canvas.status() +
                                    "; font=" + canvas.font().toString()));
                QCOMPARE(canvas.diagram()->components.size(), c.graph.components().size());
                QCOMPARE(canvas.diagram()->nets.size(), c.graph.nodes().size());
                QVERIFY(canvas.scene()->items().size() < 650);
                QImage image(800, 600, QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::transparent);
                QPainter painter(&image);
                canvas.view()->render(&painter);
                painter.end();
            }
        QCOMPARE(canvas.diagram()->crossings.size(), std::size_t(7936));
        canvas.showFailure("<b>literal</b> — Ω");
        QCOMPARE(canvas.inspection(), QString("<b>literal</b> — Ω"));
        QVERIFY(canvas.scene()->items().empty());
        QVERIFY(!canvas.selection());
        QVERIFY(canvas.setGraph(cases.front().graph));
        for (int i = 0; i < 3; ++i) {
            auto temporary = std::make_unique<q::SchematicCanvas>();
            QVERIFY(temporary->setGraph(cases[4].graph));
        }
        QApplication::processEvents();
    }
};
QTEST_MAIN(SchematicGuiTest)
#include "schematic_gui_test.moc"
