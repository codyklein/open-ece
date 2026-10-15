#include "schematic_layout_cases.hpp"
#include "schematic_preview_cases.hpp"
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QStyleOptionComboBox>
#include <QTest>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <array>
#include <schematic/qt/canvas.hpp>
#include <schematic/qt/preview_controls.hpp>
namespace s = openece::schematic;
namespace q = s::qt;
namespace {
// Inspect the actual frame paint at all four edges, independent of scene pixels,
// native-theme content, widget size and device-pixel ratio. No screenshot golden.
std::array<QColor, 4> borderSamples(QWidget* widget) {
    const auto w = widget->width(), h = widget->height();
    const std::array<QRect, 4> edges{QRect(0, 0, w, 3), QRect(0, h - 3, w, 3), QRect(0, 0, 3, h),
                                     QRect(w - 3, 0, 3, h)};
    std::array<QColor, 4> colors;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const auto image = widget->grab(edges[i]).toImage();
        colors[i] = image.pixelColor(image.width() / 2, image.height() / 2);
    }
    return colors;
}
std::array<QColor, 4> dropdownBorderSamples(QComboBox* chooser) {
    // Inspect the displayed native Windows control without asking the Windows
    // style to repaint its animated subcontrols into an offscreen widget surface.
    // Other test backends (including offscreen) use the complete widget surface.
    QPixmap frame;
    if (QApplication::platformName() == "windows") {
        QGuiApplication::sync();
        auto* window = chooser->window();
        const auto position = chooser->mapTo(window, QPoint{});
        frame = chooser->screen()->grabWindow(window->winId(), position.x(), position.y(),
                                              chooser->width(), chooser->height());
    } else {
        frame = chooser->grab();
    }
    const auto image = frame.toImage();
    if (image.isNull())
        return {};
    const auto inset = qRound(image.devicePixelRatio());
    return {image.pixelColor(image.width() / 2, inset),
            image.pixelColor(image.width() / 2, image.height() - 1 - inset),
            image.pixelColor(inset, image.height() / 2),
            image.pixelColor(image.width() - 1 - inset, image.height() / 2)};
}
} // namespace
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
    void focusTraversal_data() {
        QTest::addColumn<bool>("chooserFirst");
        QTest::addColumn<bool>("trailingControl");
        QTest::newRow("preview-parent") << true << false;
        QTest::newRow("embedded-with-following-control") << true << true;
        QTest::newRow("canvas-constructed-first") << false << true;
    }
    void focusTraversal() {
        QFETCH(bool, chooserFirst);
        QFETCH(bool, trailingControl);
        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        QComboBox* chooser = chooserFirst ? new QComboBox(&host) : nullptr;
        auto* canvas = new q::SchematicCanvas(&host);
        if (!chooser)
            chooser = new QComboBox(&host);
        layout->addWidget(chooser);
        layout->addWidget(canvas);
        auto* following =
            trailingControl ? new QPushButton("Following host control", &host) : nullptr;
        if (following)
            layout->addWidget(following);
        const auto cases = q::preview_cases();
        for (const auto& example : cases)
            chooser->addItem(example.name);
        connect(chooser, &QComboBox::currentIndexChanged, canvas, [canvas, cases](int index) {
            canvas->setGraph(cases.at(static_cast<std::size_t>(index)).graph);
        });
        auto* tree = canvas->findChild<QTreeWidget*>();
        auto* inspector = canvas->findChild<QPlainTextEdit*>();
        QVERIFY(tree);
        QVERIFY(inspector);
        QVERIFY(inspector->isReadOnly());
        QVERIFY(inspector->tabChangesFocus());
        std::array<QWidget*, 6> order{};
        for (auto* button : canvas->findChildren<QPushButton*>()) {
            if (button->text() == "Fit to View")
                order[0] = button;
            else if (button->text() == "Zoom In")
                order[1] = button;
            else if (button->text() == "Zoom Out")
                order[2] = button;
        }
        order[3] = canvas->view();
        order[4] = tree;
        order[5] = inspector;
        for (auto* widget : order)
            QVERIFY(widget);
        host.resize(1200, 850);
        host.show();
        host.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&host));
        // Host controls retain their own construction order; the canvas's six controls
        // must form a contiguous, correctly ordered group regardless of that order.
        QWidget* before = chooserFirst ? static_cast<QWidget*>(chooser) : following;
        QWidget* after = chooserFirst && following ? static_cast<QWidget*>(following) : chooser;
        for (const auto index : {0, 4, 6}) {
            chooser->setCurrentIndex(index);
            const auto& graph = cases.at(static_cast<std::size_t>(index)).graph;
            // Index zero does not emit currentIndexChanged; subsequent changes use
            // the same chooser-driven replacement path as the standalone preview.
            if (index == 0)
                QVERIFY(canvas->setGraph(graph));
            QVERIFY(canvas->diagram());
            QCOMPARE(canvas->diagram()->components.size(), graph.components().size());
            QVERIFY(canvas->select(graph.components().front().key));
            const auto selected = canvas->selection();
            const auto text = canvas->inspection();
            before->setFocus();
            QTRY_COMPARE(QApplication::focusWidget(), before);
            for (auto* expected : order) {
                QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
                QTRY_COMPARE(QApplication::focusWidget(), expected);
                QCOMPARE(canvas->selection(), selected);
            }
            // Tab leaves the read-only inspector and the canvas, including in the preview host.
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
            QTRY_COMPARE(QApplication::focusWidget(), after);
            for (auto it = order.rbegin(); it != order.rend(); ++it) {
                QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab, Qt::ShiftModifier);
                QTRY_COMPARE(QApplication::focusWidget(), *it);
                QCOMPARE(canvas->selection(), selected);
            }
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab, Qt::ShiftModifier);
            QTRY_COMPARE(QApplication::focusWidget(), before);
            QCOMPARE(canvas->inspection(), text);
            tree->setFocus();
            const auto* first = tree->currentItem();
            QTest::keyClick(tree, Qt::Key_Down);
            QVERIFY(tree->currentItem() != first);
            QVERIFY(canvas->selection() != selected);
            QTest::keyClick(tree, Qt::Key_Up);
            QCOMPARE(tree->currentItem(), first);
            QCOMPARE(canvas->selection(), selected);
        }
    }
    void visibleFocusDecoration_data() {
        QTest::addColumn<int>("selectionKind");
        QTest::newRow("unselected") << 0;
        QTest::newRow("component-selected") << 1;
        QTest::newRow("net-selected") << 2;
    }
    void visibleFocusDecoration() {
        QFETCH(int, selectionKind);
        QWidget host;
        auto palette = host.palette();
        palette.setColor(QPalette::Window, QColor("#1d2229"));
        palette.setColor(QPalette::Base, QColor("#1d2229"));
        palette.setColor(QPalette::Text, QColor("#e3e8f0"));
        host.setPalette(palette);
        auto* layout = new QVBoxLayout(&host);
        auto* outside = new QPushButton("Host control", &host);
        layout->addWidget(outside);
        auto* canvas = new q::SchematicCanvas(&host);
        layout->addWidget(canvas);
        auto* tree = canvas->findChild<QTreeWidget*>();
        auto* inspector = canvas->findChild<QPlainTextEdit*>();
        QVERIFY(tree);
        QVERIFY(inspector);
        const std::array<QAbstractScrollArea*, 3> panels{canvas->view(), tree, inspector};
        std::vector<QWidget*> order;
        for (auto* button : canvas->findChildren<QPushButton*>())
            order.push_back(button);
        QCOMPARE(order.size(), std::size_t(3));
        order.insert(order.end(), panels.begin(), panels.end());
        host.resize(1200, 850);
        host.show();
        host.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&host));
        const auto cases = q::preview_cases();
        for (const auto fixture : {0, 6}) {
            QVERIFY(canvas->setGraph(cases.at(static_cast<std::size_t>(fixture)).graph));
            // Exercise replacement plus reduced-window layout without changing the model.
            host.resize(fixture == 0 ? QSize(1200, 850) : QSize(760, 520));
            QApplication::processEvents();
            if (selectionKind == 1)
                QVERIFY(canvas->select(canvas->diagram()->components.front().source.key));
            else if (selectionKind == 2)
                QVERIFY(canvas->select(canvas->diagram()->nets.front().source.key));
            const auto selected = canvas->selection();
            const auto text = canvas->inspection();
            const auto diagram = *canvas->diagram();
            canvas->zoomIn();
            canvas->zoomIn();
            canvas->view()->horizontalScrollBar()->setValue(
                canvas->view()->horizontalScrollBar()->maximum() / 2);
            canvas->view()->verticalScrollBar()->setValue(
                canvas->view()->verticalScrollBar()->maximum() / 2);
            outside->setFocus();
            QApplication::processEvents();
            const auto transform = canvas->view()->transform();
            const QPoint scroll(canvas->view()->horizontalScrollBar()->value(),
                                canvas->view()->verticalScrollBar()->value());
            std::array<QRect, 3> bounds, viewports;
            for (std::size_t i = 0; i < panels.size(); ++i) {
                bounds[i] = panels[i]->geometry();
                viewports[i] = panels[i]->viewport()->geometry();
            }
            auto verifyDecoration = [&](QWidget* expected) {
                QTRY_COMPARE(QApplication::focusWidget(), expected);
                for (std::size_t i = 0; i < panels.size(); ++i) {
                    const auto colors = borderSamples(panels[i]);
                    for (const auto& color : colors) {
                        if (panels[i] == expected)
                            QCOMPARE(color, QColor("#96aef2"));
                        else
                            QVERIFY(color != QColor("#96aef2"));
                    }
                    QCOMPARE(panels[i]->geometry(), bounds[i]);
                    QCOMPARE(panels[i]->viewport()->geometry(), viewports[i]);
                }
                QCOMPARE(canvas->view()->transform(), transform);
                QCOMPARE(QPoint(canvas->view()->horizontalScrollBar()->value(),
                                canvas->view()->verticalScrollBar()->value()),
                         scroll);
                QCOMPARE(canvas->selection(), selected);
                QCOMPARE(canvas->inspection(), text);
                QVERIFY(*canvas->diagram() == diagram);
                QVERIFY(inspector->isReadOnly());
                const auto directory = qEnvironmentVariable("OPENECE_SCHEMATIC_SCREENSHOTS");
                if (!directory.isEmpty() && selectionKind == 1 && fixture == 0 &&
                    std::find(panels.begin(), panels.end(), expected) != panels.end()) {
                    QDir().mkpath(directory);
                    QVERIFY(
                        host.grab().save(directory + "/focus-" + expected->objectName() + ".png"));
                }
            };
            for (auto* expected : order) {
                QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
                verifyDecoration(expected);
                if (QTest::currentTestFailed())
                    return;
            }
            QTest::keyClick(inspector, Qt::Key_Tab);
            verifyDecoration(outside); // No panel keeps a stale focus outline.
            if (QTest::currentTestFailed())
                return;
            for (auto it = order.rbegin(); it != order.rend(); ++it) {
                QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab, Qt::ShiftModifier);
                verifyDecoration(*it);
                if (QTest::currentTestFailed())
                    return;
            }
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab, Qt::ShiftModifier);
            verifyDecoration(outside);
            if (QTest::currentTestFailed())
                return;
        }
    }
    void previewDropdownFocusAndSelection() {
        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        auto* chooser = q::make_preview_fixture_chooser(&host);
        auto* label = new QLabel("&Review fixture:", &host);
        label->setBuddy(chooser);
        layout->addWidget(label);
        layout->addWidget(chooser);
        auto* canvas = new q::SchematicCanvas(&host);
        layout->addWidget(canvas);
        const auto cases = q::preview_cases();
        for (const auto& example : cases)
            chooser->addItem(example.name);
        connect(chooser, &QComboBox::currentIndexChanged, canvas, [canvas, cases](int index) {
            if (index >= 0)
                canvas->setGraph(cases.at(static_cast<std::size_t>(index)).graph);
        });
        const auto available = host.screen()->availableGeometry().size() - QSize(40, 60);
        host.resize(QSize(900, 700).boundedTo(available).expandedTo(host.minimumSizeHint()));
        host.show();
        host.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&host));
        // Match preview.cpp: populate only after the viewport has a real layout.
        // Hidden, not-yet-sized native scroll areas can trigger Qt style warnings.
        QVERIFY(canvas->setGraph(cases.front().graph));
        auto* tree = canvas->findChild<QTreeWidget*>();
        auto* inspector = canvas->findChild<QPlainTextEdit*>();
        QVERIFY(tree);
        QVERIFY(inspector);
        std::vector<QWidget*> order{chooser};
        for (auto* button : canvas->findChildren<QPushButton*>())
            order.push_back(button);
        order.insert(order.end(), {canvas->view(), tree, inspector});
        QCOMPARE(order.size(), std::size_t(7));
        QCOMPARE(label->buddy(), chooser);
        QCOMPARE(label->text(), QString("&Review fixture:"));
        QCOMPARE(chooser->accessibleName(), QString("Review fixture"));
        chooser->setFocus(Qt::TabFocusReason);
        QTRY_COMPARE(QApplication::focusWidget(), chooser);
        const auto bounds = chooser->geometry();
        const auto hint = chooser->sizeHint();
        const auto diagram = *canvas->diagram();
        for (auto* expected : order) {
            if (expected != chooser)
                QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
            QTRY_COMPARE(QApplication::focusWidget(), expected);
            for (const auto& color : dropdownBorderSamples(chooser))
                QCOMPARE(color, QColor(expected == chooser ? "#96aef2" : "#46505e"));
            QCOMPARE(chooser->geometry(), bounds);
            QCOMPARE(chooser->sizeHint(), hint);
            QVERIFY(*canvas->diagram() == diagram);
        }
        QTest::keyClick(inspector, Qt::Key_Tab);
        QTRY_COMPARE(QApplication::focusWidget(), chooser);
        QTest::keyClick(chooser, Qt::Key_Tab, Qt::ShiftModifier);
        QTRY_COMPARE(QApplication::focusWidget(), inspector);
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            if (it != order.rbegin())
                QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab, Qt::ShiftModifier);
            QTRY_COMPARE(QApplication::focusWidget(), *it);
        }
        QTest::keyClick(chooser, Qt::Key_Down);
        QCOMPARE(chooser->currentIndex(), 1);
        auto matchesFixture = [&] {
            const auto& graph = cases.at(static_cast<std::size_t>(chooser->currentIndex())).graph;
            return canvas->diagram() &&
                   s::verify_layout(graph, *canvas->diagram(),
                                    q::measure_labels(graph, canvas->font()).metrics)
                       .valid();
        };
        QVERIFY(matchesFixture());
        for (const auto& color : dropdownBorderSamples(chooser))
            QCOMPARE(color, QColor("#96aef2"));
        QTest::keyClick(chooser, Qt::Key_Up);
        QCOMPARE(chooser->currentIndex(), 0);
        QVERIFY(matchesFixture());

        QStyleOptionComboBox option;
        option.initFrom(chooser);
        option.currentText = chooser->currentText();
        const auto arrow = chooser->style()->subControlRect(QStyle::CC_ComboBox, &option,
                                                            QStyle::SC_ComboBoxArrow, chooser);
        QVERIFY(!arrow.isEmpty());
        QVERIFY(chooser->rect().contains(arrow.center()));
        auto movePointer = [](QWidget* target, QPoint position) {
            // The QWidget overload relies on OS cursor movement. QWindow sends a
            // deterministic Qt mouse event, including on native Windows runners.
            auto* window = target->window();
            QTest::mouseMove(window->windowHandle(), target->mapTo(window, position));
        };
        // Open using the real arrow hit area; Escape must restore usable keyboard focus.
        movePointer(chooser, arrow.center());
        QTest::mouseClick(chooser, Qt::LeftButton, Qt::NoModifier, arrow.center());
        QTRY_VERIFY(chooser->view()->isVisible());
        QVERIFY(QTest::qWaitForWindowExposed(chooser->view()->window()));
        QTest::keyClick(chooser->view(), Qt::Key_Escape);
        QTRY_VERIFY(!chooser->view()->isVisible());
        QVERIFY(QTest::qWaitForWindowActive(&host));
        QTRY_COMPARE(QApplication::focusWidget(), chooser);
        QTest::keyClick(chooser, Qt::Key_Tab);
        QTRY_COMPARE(QApplication::focusWidget(), order[1]);
        QTest::keyClick(order[1], Qt::Key_Tab, Qt::ShiftModifier);
        QTRY_COMPARE(QApplication::focusWidget(), chooser);

        // Mouse selection must still replace the fixture through the preview's callback.
        movePointer(chooser, arrow.center());
        QTest::mouseClick(chooser, Qt::LeftButton, Qt::NoModifier, arrow.center());
        QTRY_VERIFY(chooser->view()->isVisible());
        QVERIFY(QTest::qWaitForWindowExposed(chooser->view()->window()));
        const auto index = chooser->model()->index(4, chooser->modelColumn());
        chooser->view()->scrollTo(index);
        const auto row = chooser->view()->visualRect(index);
        QVERIFY(!row.isEmpty());
        // Follow the real pointer path from arrow to row. Qt guards against the
        // opening click's release selecting an item before that movement occurs.
        movePointer(chooser->view()->viewport(), row.center());
        QApplication::processEvents();
        QTest::mousePress(chooser->view()->viewport(), Qt::LeftButton, Qt::NoModifier,
                          row.center());
        QTRY_COMPARE(chooser->view()->currentIndex(), index);
        QTest::mouseRelease(chooser->view()->viewport(), Qt::LeftButton, Qt::NoModifier,
                            row.center());
        QTRY_COMPARE(chooser->currentIndex(), 4);
        QTRY_VERIFY(!chooser->view()->isVisible());
        QVERIFY(QTest::qWaitForWindowActive(&host));
        QVERIFY(matchesFixture());
        host.resize(
            QSize(host.width() - 60, host.height() - 30).expandedTo(host.minimumSizeHint()));
        QApplication::processEvents();
        const auto resizedBounds = chooser->geometry();
        canvas->view()->setFocus();
        for (const auto& color : dropdownBorderSamples(chooser))
            QCOMPARE(color, QColor("#46505e"));
        QTest::keyClick(canvas->view(), Qt::Key_R, Qt::AltModifier);
        QTRY_COMPARE(QApplication::focusWidget(), chooser); // Existing label-buddy shortcut.
        QCOMPARE(chooser->geometry(), resizedBounds);
        for (const auto& color : dropdownBorderSamples(chooser))
            QCOMPARE(color, QColor("#96aef2"));
        option.initFrom(chooser);
        option.currentText = chooser->currentText();
        const auto text = chooser->style()->subControlRect(QStyle::CC_ComboBox, &option,
                                                           QStyle::SC_ComboBoxEditField, chooser);
        QVERIFY(text.width() >=
                QFontMetrics(chooser->font()).horizontalAdvance(option.currentText));
        QVERIFY(text.height() >= QFontMetrics(chooser->font()).height());
        QVERIFY(chooser->rect().contains(text));
        const auto directory = qEnvironmentVariable("OPENECE_SCHEMATIC_SCREENSHOTS");
        if (!directory.isEmpty()) {
            QDir().mkpath(directory);
            QVERIFY(host.grab().save(directory + "/preview-dropdown-focused.png"));
        }
    }
    void inspectorKeyboardScrolling() {
        q::SchematicCanvas canvas;
        canvas.resize(1100, 700);
        canvas.show();
        canvas.activateWindow();
        // Native Windows activation is asynchronous; shortcut delivery requires
        // an active window, not just QApplication's logical focus widget.
        QVERIFY(QTest::qWaitForWindowActive(&canvas));
        const auto graph = s::project_schematic(schematic_cases::large_sources());
        QVERIFY(canvas.setGraph(graph));
        QVERIFY(canvas.select(graph.components().front().key));
        auto* inspector = canvas.findChild<QPlainTextEdit*>();
        inspector->setFocus();
        QTRY_COMPARE(QApplication::focusWidget(), inspector);
        QVERIFY(inspector->isReadOnly());
        const auto text = inspector->toPlainText();
        QTest::keyClick(inspector, Qt::Key_X);
        QCOMPARE(inspector->toPlainText(), text);
        QTest::keyClick(inspector, Qt::Key_Home, Qt::ControlModifier);
        auto* bar = inspector->verticalScrollBar();
        QTRY_VERIFY(bar->maximum() > 0);
        QCOMPARE(bar->value(), 0);
        QTest::keyClick(inspector, Qt::Key_PageDown);
        QVERIFY(bar->value() > 0);
        QTest::keyClick(inspector, Qt::Key_PageUp);
        QCOMPARE(bar->value(), 0);
        QCOMPARE(inspector->toPlainText(), text);
        const auto scale = canvas.view()->transform().m11();
        QTest::keyClick(inspector, Qt::Key_Plus);
        QVERIFY(canvas.view()->transform().m11() > scale);
        QTest::keyClick(inspector, Qt::Key_Minus);
        QVERIFY(std::abs(canvas.view()->transform().m11() - scale) < .001);
        QTest::keyClick(inspector, Qt::Key_F);
        QCOMPARE(inspector->toPlainText(), text);
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
