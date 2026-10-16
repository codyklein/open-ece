#include "ac_view.hpp"
#include "circuit_presentation.hpp"
#include "circuits_view.hpp"
#include "project_document.hpp"
#include "schematic_layout_cases.hpp"
#include <QComboBox>
#include <QGraphicsView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest>
#include <schematic/qt/canvas.hpp>
using namespace openece;
using namespace openece::gui;
namespace s = schematic;
namespace {
template <class T> T* get(QObject& root, const char* name) {
    auto* p = root.findChild<T*>(name);
    if (!p)
        qFatal("Missing %s", name);
    return p;
}
CircuitPresentation* presentation(QObject& root, const char* prefix) {
    return dynamic_cast<CircuitPresentation*>(
        root.findChild<QWidget*>(QString(prefix) + "_presentation"));
}
void choose(QObject& root, const char* prefix, int index) {
    get<QComboBox>(root, (QString(prefix) + "_circuit_view").toUtf8().constData())
        ->setCurrentIndex(index);
}
bool matches(CircuitPresentation* p, const s::SchematicGraph& graph) {
    return p && p->canvas() && p->canvas()->diagram() &&
           s::verify_layout(graph, *p->canvas()->diagram(),
                            s::qt::measure_labels(graph, p->canvas()->font()).metrics)
               .valid();
}
void expose(QWidget& view) {
    view.resize(1100, 850);
    view.show();
    view.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&view));
}
} // namespace
class SchematicWorkspaceTests final : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void dcNavigationAndRefresh() {
        auto draft = schematic_cases::divider();
        CircuitsView view(nullptr, &draft, true);
        expose(view);
        const auto original = draft;
        auto* panel = presentation(view, "dc");
        QVERIFY(panel);
        QVERIFY(!panel->canvas()); // Restoration/construction creates no schematic scene.
        auto* tabs = get<QTabWidget>(view, "dc_view_tabs");
        QCOMPARE(tabs->count(), 2);
        QSignalSpy edits(&view, &DraftView::draftEdited);
        choose(view, "dc", 1);
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
        QVERIFY(draft == original);
        QCOMPARE(edits.count(), 0);
        auto* canvas = panel->canvas();
        auto* switcher = get<QComboBox>(view, "dc_circuit_view");
        std::vector<QWidget*> focus;
        for (auto* button : canvas->findChildren<QPushButton*>())
            focus.push_back(button);
        focus.insert(focus.end(), {canvas->view(), canvas->findChild<QTreeWidget*>(),
                                   canvas->findChild<QPlainTextEdit*>()});
        switcher->setFocus();
        for (auto* expected : focus) {
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
            QCOMPARE(QApplication::focusWidget(), expected);
        }
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
        QVERIFY(!canvas->isAncestorOf(QApplication::focusWidget()));
        for (auto it = focus.rbegin(); it != focus.rend(); ++it) {
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab, Qt::ShiftModifier);
            QCOMPARE(QApplication::focusWidget(), *it);
        }
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab, Qt::ShiftModifier);
        QCOMPARE(QApplication::focusWidget(), switcher);
        canvas->zoomIn();
        const auto transform = canvas->view()->transform();
        QVERIFY(canvas->select(s::ComponentKey{s::Analysis::dc, draft.components[1].id.value}));
        choose(view, "dc", 0);
        choose(view, "dc", 1);
        QCoreApplication::processEvents();
        QCOMPARE(canvas->view()->transform(), transform);
        QCOMPARE(edits.count(), 0);
        choose(view, "dc", 0);
        auto* nodes = get<QTableWidget>(view, "circuit_nodes");
        nodes->item(1, 1)->setText("Renamed <b>π</b>");
        get<QPushButton>(view, "circuit_add_node")->click();
        get<QPushButton>(view, "circuit_add_component")->click();
        auto* parts = get<QTableWidget>(view, "circuit_components");
        const int last = parts->rowCount() - 1;
        const auto added = draft.components.back().id;
        auto* positive = static_cast<QComboBox*>(parts->cellWidget(last, 3));
        positive->setCurrentIndex(positive->findData(draft.nodes.back().id.value));
        auto* ground = get<QComboBox>(view, "circuit_ground");
        ground->setCurrentIndex(ground->findData(draft.nodes.back().id.value));
        choose(view, "dc", 1);
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
        QVERIFY(canvas->select(s::ComponentKey{s::Analysis::dc, added.value}));
        // Real hidden editor notifications refresh a currently displayed schematic.
        static_cast<QLineEdit*>(parts->cellWidget(last, 5))->setText("1e-");
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
        QCOMPARE(canvas->selection(),
                 std::optional<s::LabelOwner>(s::ComponentKey{s::Analysis::dc, added.value}));
        QVERIFY(canvas->inspection().contains("1e-"));
        parts->setCurrentCell(last, 1);
        get<QPushButton>(view, "circuit_remove_component")->click();
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
        QVERIFY(!canvas->selection());
        nodes->setCurrentCell(nodes->rowCount() - 1, 1);
        get<QPushButton>(view, "circuit_remove_node")->click();
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
        const auto graph = s::project_schematic(draft);
        QVERIFY(std::holds_alternative<s::DanglingNode>(graph.ground()));
    }
    void dcRefreshFailureClearsPreviousScene() {
        auto draft = schematic_cases::divider();
        CircuitsView view(nullptr, &draft, true);
        expose(view);
        choose(view, "dc", 1);
        auto* panel = presentation(view, "dc");
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
        // Deliberate structural failure through an external draft change notification.
        draft.nodes.push_back(draft.nodes.front());
        const auto invalid = draft;
        Q_EMIT view.draftEdited();
        QTRY_VERIFY(!panel->canvas()->diagram());
        QVERIFY(panel->canvas()->status().contains("unavailable"));
        QVERIFY(draft == invalid);
        draft.nodes.pop_back();
        Q_EMIT view.draftEdited();
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
    }
    void documentNavigationIsNotAnEdit() {
        auto snapshot = project::default_project();
        snapshot.selected_domain = "circuits";
        ProjectDocument document(std::make_unique<ProjectWorkspace>(snapshot, true));
        auto& workspace = document.workspace();
        expose(workspace);
        choose(workspace, "dc", 1);
        auto* panel = presentation(workspace, "dc");
        QTRY_VERIFY(matches(panel, s::project_schematic(snapshot.circuits.dc)));
        panel->canvas()->fit();
        panel->canvas()->zoomIn();
        panel->canvas()->zoomOut();
        panel->canvas()->view()->setFocus();
        panel->canvas()->select(s::NodeKey{s::Analysis::dc, 0});
        choose(workspace, "dc", 0);
        QVERIFY(workspace.capture() == snapshot);
        QVERIFY(!document.dirty());
        auto* parts = get<QTableWidget>(workspace, "circuit_components");
        static_cast<QLineEdit*>(parts->cellWidget(1, 5))->setText("1e-");
        QVERIFY(document.dirty());
    }
    void dcPendingDelegateAndResults() {
        auto draft = schematic_cases::divider();
        CircuitsView view(nullptr, &draft, true);
        expose(view);
        get<QPushButton>(view, "circuit_solve")->click();
        auto* volts = get<QTableWidget>(view, "circuit_voltages");
        QCOMPARE(volts->rowCount(), 3);
        const auto result = volts->item(2, 1)->text();
        choose(view, "dc", 1);
        auto* panel = presentation(view, "dc");
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
        QCOMPARE(volts->item(2, 1)->text(), result);
        choose(view, "dc", 0);
        auto* parts = get<QTableWidget>(view, "circuit_components");
        auto* value = static_cast<QLineEdit*>(parts->cellWidget(1, 5));
        value->setFocus();
        value->selectAll();
        QTest::keyClicks(value, " 1e- ");
        QCOMPARE(volts->rowCount(), 0); // Genuine edit retains existing invalidation.
        auto* item = parts->item(1, 1);
        parts->setCurrentItem(item);
        parts->editItem(item);
        QCoreApplication::processEvents();
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        editor->setText("  pending Ω  "); // No textEdited/commit yet.
        choose(view, "dc", 1);
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
        QCOMPARE(draft.components[1].name, std::string("  pending Ω  "));
        QCOMPARE(draft.components[1].value.text, std::string(" 1e- "));
        QVERIFY(panel->canvas()->select(
            s::ComponentKey{s::Analysis::dc, draft.components[1].id.value}));
        QVERIFY(panel->canvas()->inspection().contains(" 1e- "));
        choose(view, "dc", 0);
        QCOMPARE(value->text(), QString(" 1e- "));
        QCOMPARE(parts->item(1, 1)->text(), QString("  pending Ω  "));
    }
    void acRawDraftAndSolvedResults() {
        auto draft = schematic_cases::example("rc-lowpass").circuits.ac;
        AcView view(nullptr, &draft, true);
        expose(view);
        auto* tabs = get<QTabWidget>(view, "ac_view_tabs");
        QCOMPARE(tabs->count(), 4);
        choose(view, "ac", 1);
        auto* panel = presentation(view, "ac");
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
        get<QPushButton>(view, "ac_solve")->click();
        auto* volts = get<QTableWidget>(view, "ac_voltages");
        QCOMPARE(volts->rowCount(), 3);
        const auto result = volts->item(2, 4)->text();
        tabs->setCurrentIndex(0);
        QSignalSpy edits(&view, &DraftView::draftEdited);
        const auto before = draft;
        choose(view, "ac", 0);
        choose(view, "ac", 1);
        QCoreApplication::processEvents();
        QCOMPARE(edits.count(), 0);
        QVERIFY(draft == before);
        QCOMPARE(volts->item(2, 4)->text(), result);
        auto* parts = get<QTableWidget>(view, "ac_components");
        static_cast<QLineEdit*>(parts->cellWidget(0, 7))->setText(" phase <b>π</b> ");
        static_cast<QLineEdit*>(parts->cellWidget(0, 5))->setText(" 1e- ");
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
        QVERIFY(panel->canvas()->select(
            s::ComponentKey{s::Analysis::ac, draft.components[0].id.value}));
        QVERIFY(panel->canvas()->inspection().contains(" phase <b>π</b> "));
        QVERIFY(panel->canvas()->inspection().contains(" 1e- "));
        QCOMPARE(volts->rowCount(), 0);
        choose(view, "ac", 0);
        QCOMPARE(static_cast<QLineEdit*>(parts->cellWidget(0, 7))->text(),
                 QString(" phase <b>π</b> "));
        get<QPushButton>(view, "ac_add_node")->click();
        auto* nodes = get<QTableWidget>(view, "ac_nodes");
        nodes->item(nodes->rowCount() - 1, 1)->setText("unused Ω");
        get<QPushButton>(view, "ac_add_component")->click();
        auto* positive = static_cast<QComboBox*>(parts->cellWidget(parts->rowCount() - 1, 3));
        positive->setCurrentIndex(positive->findData(draft.nodes.back().id.value));
        get<QComboBox>(view, "ac_ground")->setCurrentIndex(0);
        choose(view, "ac", 1);
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
        nodes->setCurrentCell(nodes->rowCount() - 1, 1);
        get<QPushButton>(view, "ac_remove_node")->click();
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
    }
    void acSweepDoesNotDriveSchematicOrStopOnNavigation() {
        auto draft = schematic_cases::example("rc-lowpass").circuits.ac;
        AcView view(nullptr, &draft, true);
        expose(view);
        auto* tabs = get<QTabWidget>(view, "ac_view_tabs");
        get<QSpinBox>(view, "ac_point_count")->setValue(41);
        choose(view, "ac", 1);
        auto* panel = presentation(view, "ac");
        QTRY_VERIFY(matches(panel, s::project_schematic(draft)));
        panel->canvas()->zoomIn();
        const auto transform = panel->canvas()->view()->transform();
        QSignalSpy edits(&view, &DraftView::draftEdited);
        get<QPushButton>(view, "ac_run_sweep")->click();
        auto* timer = view.findChildren<QTimer*>(QString{}, Qt::FindDirectChildrenOnly).front();
        QVERIFY(timer->isActive());
        tabs->setCurrentIndex(0);
        choose(view, "ac", 0);
        choose(view, "ac", 1);
        QVERIFY(timer->isActive());
        QTRY_VERIFY_WITH_TIMEOUT(
            get<QLabel>(view, "ac_status")->text().startsWith("Sweep complete"), 10000);
        QCOMPARE(panel->canvas()->view()->transform(), transform);
        QCOMPARE(edits.count(), 0);
        const auto complete = get<QLabel>(view, "ac_status")->text();
        choose(view, "ac", 0);
        choose(view, "ac", 1);
        QCOMPARE(get<QLabel>(view, "ac_status")->text(), complete);
        get<QSpinBox>(view, "ac_point_count")->setValue(1001);
        get<QPushButton>(view, "ac_run_sweep")->click();
        tabs->setCurrentIndex(0);
        QTimer::singleShot(0, &view, [&] {
            choose(view, "ac", 0);
            choose(view, "ac", 1);
            get<QPushButton>(view, "ac_cancel")->click();
        });
        QTRY_VERIFY(get<QLabel>(view, "ac_status")->text().startsWith("Sweep cancelled"));
        const auto cancelled = get<QLabel>(view, "ac_status")->text();
        choose(view, "ac", 0);
        choose(view, "ac", 1);
        QCoreApplication::processEvents();
        QCOMPARE(get<QLabel>(view, "ac_status")->text(), cancelled);
    }
};
QTEST_MAIN(SchematicWorkspaceTests)
#include "schematic_workspace_test.moc"
