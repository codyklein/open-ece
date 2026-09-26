#include "ac_view.hpp"
#include "circuits_view.hpp"
#include "communications_view.hpp"
#include "digital_logic_view.hpp"
#include "project_workspace.hpp"
#include "signals_dsp_view.hpp"
#include "timing_view.hpp"
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPersistentModelIndex>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QtTest>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
using namespace openece;
using namespace openece::gui;
namespace {
template <class T> T* control(QObject& root, const char* name) {
    auto* p = root.findChild<T*>(name);
    if (!p)
        qFatal("Missing control: %s", name);
    return p;
}
void click(QObject& root, const char* name) { control<QPushButton>(root, name)->click(); }
project::ProjectSnapshot invalid_draft() {
    auto p = project::default_project();
    p.selected_domain = "communications";
    p.signals.amplitude.text = "";
    p.signals.frequency.text = " 1e- ";
    p.signals.phase = {" π / \t", "rad"};
    p.signals.taps_text = "bad";
    p.signals.selected_tab = "response";
    p.digital.combinational.inputs[0].name = " α \t🚀";
    p.digital.combinational.gates[0].pins = {project::Id{99}, std::nullopt, project::Id{1}};
    p.digital.combinational.gates[0].pin_count_text = "1e-";
    p.digital.combinational.outputs[0].source = project::Id{100};
    p.digital.combinational.next_id = {80};
    p.digital.timing.elements[0].pins_text = " 1,,99, π ";
    p.digital.timing.elements[0].delay.text = "";
    p.digital.timing.inputs[1].clock.high_text = "1e-";
    p.digital.timing.horizon.text = "";
    p.digital.timing.selected_tab = "stimuli";
    p.digital.timing.display_unit = "us";
    p.digital.copy_delay.text = "";
    p.digital.selected_tab = "timing";
    p.circuits.dc.nodes[0].name = "";
    p.circuits.dc.ground = project::Id{999};
    p.circuits.dc.components[0].positive.reset();
    p.circuits.dc.components[1].value = {" 1e- ", "kohm"};
    p.circuits.dc.selected_tab = "help";
    p.circuits.ac.components[1].phase.text = " disabled π ";
    p.circuits.ac.components[2].value = {"", "pF"};
    p.circuits.ac.probe_negative = project::Id{123};
    p.circuits.ac.reference_source = project::Id{456};
    p.circuits.ac.frequency = {"1e-", "MHz"};
    p.circuits.ac.count_text = "odd";
    p.circuits.ac.selected_tab = "sweep";
    p.circuits.selected_tab = "ac";
    p.communications.manual_bits_text = " 01 1\tπ🚀 ";
    p.communications.bit_count_text = "3";
    p.communications.noise_seed_text = "18446744073709551616";
    p.communications.bit_seed_text = " 1e- ";
    p.communications.budget_text = "";
    p.communications.noise_enabled = false;
    p.communications.selected_tab = "ber";
    return p;
}
void empty_results(QObject& w) {
    for (auto name :
         {"digital_truth_table", "timing_results", "circuit_voltages", "circuit_currents",
          "ac_voltages", "ac_currents", "ac_sweep_results", "comm_bits", "comm_ber_results"}) {
        auto* table = w.findChild<QTableWidget*>(name);
        if (table)
            QCOMPARE(table->rowCount(), 0);
    }
    for (auto* timer : w.findChildren<QTimer*>())
        QVERIFY(!timer->isActive());
    for (auto* plot : w.findChildren<QwtPlot*>())
        for (auto* item : plot->itemList(QwtPlotItem::Rtti_PlotCurve))
            QCOMPARE(static_cast<QwtPlotCurve*>(item)->dataSize(), std::size_t{0});
}
QLineEdit* active_editor(QTableWidget* table, int row, int col) {
    table->setCurrentCell(row, col);
    table->editItem(table->item(row, col));
    QCoreApplication::processEvents();
    for (auto* e : table->findChildren<QLineEdit*>())
        if (e->property("draftIndex").isValid())
            return e;
    qFatal("No active table editor");
    return nullptr;
}
} // namespace
class ProjectGuiTest : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void defaultRoundTripAndInertResults() {
        ProjectWorkspace original;
        QSignalSpy changes(&original, &DraftView::draftEdited);
        auto p = original.capture();
        QVERIFY(p == project::default_project());
        empty_results(original);
        ProjectWorkspace restored(project::decode_project(project::encode_project(p)).snapshot);
        restored.show();
        QCoreApplication::processEvents();
        QVERIFY(restored.capture() == p);
        empty_results(restored);
        QCOMPARE(changes.count(), 0);
    }
    void invalidCrossDomainRoundTrip() {
        const auto p = invalid_draft();
        ProjectWorkspace view(p);
        QSignalSpy changes(&view, &DraftView::draftEdited);
        view.show();
        QCoreApplication::processEvents();
        QVERIFY(view.capture() == p);
        empty_results(view);
        QCOMPARE(changes.count(), 0);
        view.hide();
        view.show();
        QCoreApplication::processEvents();
        QVERIFY(view.capture() == p);
        ProjectWorkspace restored(
            project::decode_project(project::encode_project(view.capture())).snapshot);
        QVERIFY(restored.capture() == p);
        empty_results(restored);
    }
    void individualDomains_data() {
        QTest::addColumn<int>("domain");
        for (int i = 0; i < 6; ++i)
            QTest::newRow(qPrintable(QString::number(i))) << i;
    }
    void individualDomains() {
        QFETCH(int, domain);
        auto p = invalid_draft();
        auto expected = p;
        std::unique_ptr<DraftView> v;
        switch (domain) {
        case 0:
            v = std::make_unique<SignalsDspView>(nullptr, &p.signals, true);
            break;
        case 1:
            v = std::make_unique<DigitalLogicView>(nullptr, &p.digital.combinational, true);
            break;
        case 2:
            v = std::make_unique<TimingView>(nullptr, &p.digital.timing, true);
            break;
        case 3:
            v = std::make_unique<CircuitsView>(nullptr, &p.circuits.dc, true);
            break;
        case 4:
            v = std::make_unique<AcView>(nullptr, &p.circuits.ac, true);
            break;
        default:
            v = std::make_unique<CommunicationsView>(nullptr, &p.communications, true);
        }
        QSignalSpy changes(v.get(), &DraftView::draftEdited);
        v->show();
        QCoreApplication::processEvents();
        v->synchronize_pending_text();
        QVERIFY(p == expected);
        empty_results(*v);
        QCOMPARE(changes.count(), 0);
    }
    void spinPendingTextSurvivesFocusHideAndCapture() {
        ProjectWorkspace w;
        w.show();
        auto* spin = control<QDoubleSpinBox>(w, "frequency");
        auto* e = spin->findChild<QLineEdit*>();
        e->setFocus();
        e->setText(" 1e- ");
        const auto visible = e->text();
        const auto p = w.capture();
        QCOMPARE(p.signals.frequency.text, std::string(" 1e- "));
        QCOMPARE(e->text(), visible);
        control<QLineEdit>(w, "phase")->setFocus();
        QCoreApplication::processEvents();
        QVERIFY(w.capture() == p);
        w.hide();
        w.show();
        QCoreApplication::processEvents();
        QVERIFY(w.capture() == p);
        click(w, "generate");
        QCOMPARE(w.capture().signals.frequency.text, std::string(" 1e- "));
        QVERIFY(control<QLabel>(w, "status")->text().contains("Cannot generate"));
    }
    void activeDelegateSynchronizesWithoutCommittingOrParsing() {
        auto p = project::default_project();
        p.selected_domain = "digital";
        p.digital.selected_tab = "timing";
        ProjectWorkspace w(p);
        w.show();
        auto* table = control<QTableWidget>(w, "timing_inputs");
        auto before = table->item(0, 3)->text();
        auto* e = active_editor(table, 0, 3);
        e->setText(" 1e-π ");
        QCOMPARE(table->item(0, 3)->text(), before);
        auto captured = w.capture();
        QCOMPARE(captured.digital.timing.inputs[0].clock.first_edge_text, std::string(" 1e-π "));
        QCOMPARE(table->item(0, 3)->text(), before);
        QCOMPARE(e->text(), QString(" 1e-π "));
        QTest::keyClick(e, Qt::Key_Escape);
        QCoreApplication::processEvents();
        QCOMPARE(w.capture().digital.timing.inputs[0].clock.first_edge_text, draft_text(before));
    }
    void unitEditsAndInvalidRollbackUpdateModel() {
        auto p = project::default_project();
        CircuitsView dc(nullptr, &p.circuits.dc, true);
        auto* table = control<QTableWidget>(dc, "circuit_components");
        auto* value = static_cast<QLineEdit*>(table->cellWidget(1, 5));
        auto* unit = static_cast<QComboBox*>(table->cellWidget(1, 6));
        unit->setCurrentIndex(1);
        QCOMPARE(p.circuits.dc.components[1].value.unit, std::string("kohm"));
        QCOMPARE(p.circuits.dc.components[1].value.text, std::string("1"));
        value->setText("1e-");
        unit->setCurrentIndex(2);
        QCOMPARE(unit->currentIndex(), 1);
        dc.synchronize_pending_text();
        QCOMPARE(p.circuits.dc.components[1].value.text, std::string("1e-"));
        QCOMPARE(p.circuits.dc.components[1].value.unit, std::string("kohm"));
    }
    void modelEditsAreImmediateAndResultsDoNotEdit() {
        ProjectWorkspace w;
        QSignalSpy edits(&w, &DraftView::draftEdited);
        control<QLineEdit>(w, "phase")->setText("30");
        QCOMPARE(w.draft().signals.phase.text, std::string("30"));
        QVERIFY(edits.count() > 0);
        edits.clear();
        const auto p = w.capture();
        for (auto name : {"generate", "digital_evaluate", "digital_truth", "timing_step",
                          "circuit_solve", "ac_solve", "comm_simulate", "comm_step", "comm_cancel"})
            click(w, name);
        QVERIFY(w.capture() == p);
        QCOMPARE(edits.count(), 0);
    }
    void allocationSkipsDanglingReferencesAfterRestore() {
        auto p = project::default_project();
        p.digital.combinational.outputs[0].source = project::Id{5};
        p.digital.timing.elements[0].pins_text = "4, bad5";
        p.circuits.dc.ground = project::Id{3};
        p.circuits.ac.reference_source = project::Id{3};
        ProjectWorkspace w(p);
        click(w, "digital_add_input");
        click(w, "timing_inputs_add");
        click(w, "circuit_add_node");
        click(w, "ac_add_component");
        auto s = w.capture();
        QCOMPARE(s.digital.combinational.inputs.back().id.value, 6U);
        QCOMPARE(s.digital.timing.inputs.back().id.value, 6U);
        QCOMPARE(s.circuits.dc.nodes.back().id.value, 4U);
        QCOMPARE(s.circuits.ac.components.back().id.value, 4U);
        QCOMPARE(s.circuits.dc.ground, project::Reference{project::Id{3}});
    }
    void orderingHighIdsAndNoAutomaticRepair() {
        auto p = invalid_draft();
        std::reverse(p.circuits.dc.nodes.begin(), p.circuits.dc.nodes.end());
        std::reverse(p.digital.timing.stimuli.begin(), p.digital.timing.stimuli.end());
        p.circuits.ac.next_node = {project::limits::exhausted_id};
        p.circuits.ac.nodes.back().id = {0xffffffffU};
        p.digital.combinational.gates[1].pins.clear();
        p.digital.combinational.gates[1].pin_count_text = "";
        ProjectWorkspace w(p);
        QVERIFY(w.capture() == p);
        click(w, "ac_add_node");
        QVERIFY(w.capture() == p);
    }
    void selectedDomainTabsAndModesArePersisted() {
        ProjectWorkspace w;
        control<QListWidget>(w, "domain_navigation")->setCurrentRow(2);
        control<QTabWidget>(w, "circuits_analysis_tabs")->setCurrentIndex(1);
        control<QTabWidget>(w, "ac_view_tabs")->setCurrentIndex(3);
        control<QComboBox>(w, "ac_response_mode")->setCurrentIndex(1);
        auto p = w.capture();
        QCOMPARE(p.selected_domain, std::string("circuits"));
        QCOMPARE(p.circuits.selected_tab, std::string("ac"));
        QCOMPARE(p.circuits.ac.selected_tab, std::string("help"));
        QCOMPARE(p.circuits.ac.mode, std::string("absolute"));
        ProjectWorkspace restored(p);
        QVERIFY(restored.capture() == p);
    }
    void repairedCircuitStillRunsFromTheDraft() {
        auto p = project::default_project();
        AcView v(nullptr, &p.circuits.ac, true);
        auto* nodes = control<QTableWidget>(v, "ac_nodes");
        auto* parts = control<QTableWidget>(v, "ac_components");
        nodes->selectRow(2);
        click(v, "ac_remove_node");
        click(v, "ac_add_node");
        for (auto rc : {std::pair{1, 4}, std::pair{2, 3}}) {
            auto* box = static_cast<QComboBox*>(parts->cellWidget(rc.first, rc.second));
            box->setCurrentIndex(box->findData(3u));
        }
        auto* probe = control<QComboBox>(v, "ac_probe_positive");
        probe->setCurrentIndex(probe->findData(3u));
        control<QSpinBox>(v, "ac_point_count")->setValue(2);
        click(v, "ac_run_sweep");
        QTRY_VERIFY2(control<QLabel>(v, "ac_status")->text().startsWith("Sweep complete"),
                     qPrintable(control<QLabel>(v, "ac_status")->text()));
    }
    void storageLimitsDoNotTruncateUnicode() {
        auto p = project::default_project();
        p.communications.manual_bits_text = std::string(131072, '1');
        p.signals.phase.text = std::string(32767, ' ');
        p.circuits.ac.components[0].phase.text.clear();
        for (int i = 0; i < 64; ++i)
            p.circuits.ac.components[0].phase.text += "🚀";
        ProjectWorkspace w(p);
        QVERIFY(w.capture() == p);
        QCOMPARE(control<QLineEdit>(w, "comm_manual")->text().size(), 131072);
        auto* table = control<QTableWidget>(w, "ac_components");
        QCOMPARE(static_cast<QLineEdit*>(table->cellWidget(0, 7))->text().size(), 128);
    }
    void exhaustedAllocatorsAreReportedWithoutEditing() {
        auto p = project::default_project();
        constexpr std::uint64_t exhausted = std::uint64_t{1} << 32;
        p.digital.combinational.next_id = {exhausted};
        p.digital.timing.next_id = {exhausted};
        p.circuits.dc.next_node = p.circuits.dc.next_component = {exhausted};
        p.circuits.ac.next_node = p.circuits.ac.next_component = {exhausted};
        ProjectWorkspace w(p);
        QSignalSpy edits(&w, &DraftView::draftEdited);
        for (auto name :
             {"digital_add_input", "digital_add_gate", "timing_inputs_add", "timing_elements_add",
              "circuit_add_node", "circuit_add_component", "ac_add_node", "ac_add_component"})
            click(w, name);
        QVERIFY(w.capture() == p);
        QCOMPARE(edits.count(), 0);
    }
    void executionUsesPendingTableText() {
        auto p = project::default_project();
        p.selected_domain = "digital";
        p.digital.selected_tab = "timing";
        ProjectWorkspace w(p);
        w.show();
        auto* table = control<QTableWidget>(w, "timing_inputs");
        auto* e = active_editor(table, 0, 2);
        const auto original = table->item(0, 2)->text();
        e->setText("1e-");
        click(w, "timing_step");
        QVERIFY(control<QLabel>(w, "timing_status")->text().startsWith("Cannot simulate"));
        QCOMPARE(table->item(0, 2)->text(), original);
        QCOMPARE(w.draft().digital.timing.inputs[0].initial_text, std::string("1e-"));
        empty_results(w);
        w.close();
        QCOMPARE(w.capture().digital.timing.inputs[0].initial_text, std::string("1e-"));
    }
    void emptyDraftsRemainEmpty() {
        project::ProjectSnapshot p;
        ProjectWorkspace w(p);
        QVERIFY(w.capture() == p);
        QCOMPARE(control<QTableWidget>(w, "circuit_nodes")->rowCount(), 0);
        QCOMPARE(control<QTableWidget>(w, "digital_gates")->rowCount(), 0);
        empty_results(w);
    }
};
QTEST_MAIN(ProjectGuiTest)
#include "project_gui_test.moc"
