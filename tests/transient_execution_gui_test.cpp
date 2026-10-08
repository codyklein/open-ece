#include "project_document.hpp"
#include "transient_results.hpp"
#include "transient_view.hpp"
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <qwt_plot_curve.h>
using namespace openece;
using namespace openece::gui;
namespace tr = circuits::transient;
namespace {
template <class T> T* control(QObject& w, const char* name) {
    auto* p = w.findChild<T*>(name);
    if (!p)
        qFatal("Missing %s", name);
    return p;
}
project::TransientDraft rc() {
    project::TransientDraft d;
    d.nodes = {{{0}, "Ground"}, {{1}, "Supply"}, {{2}, "Output π"}};
    d.ground = project::Id{0};
    d.next_node = {3};
    d.next_component = {13};
    d.components = {{{10}, "V", "voltage_source", project::Id{1}, project::Id{0}, {"10", "V"}, {}},
                    {{11}, "R", "resistor", project::Id{1}, project::Id{2}, {"1", "kohm"}, {}},
                    {{12}, "C", "capacitor", project::Id{2}, project::Id{0}, {"1", "uF"}, {}}};
    d.stop = {"5", "ms"};
    d.maximum_step = {"0.1", "ms"};
    d.display_time_unit = "ms";
    d.initialization = "specified_storage";
    d.initial_conditions = {{project::Id{12}, "capacitor_voltage", {"0", "V"}}};
    // Mixed order, with inactive dangling fields deliberately retained.
    d.probes = {{"Supply current", "current", project::Id{900}, std::nullopt, project::Id{10}},
                {"Output <b>π</b>", "voltage", project::Id{2}, project::Id{0}, project::Id{901}},
                {"Cap current", "current", std::nullopt, std::nullopt, project::Id{12}}};
    return d;
}
void click(QObject& w, const char* name) { control<QPushButton>(w, name)->click(); }
bool state(QObject& w, const char* text) {
    return control<QLabel>(w, "transient_status")->text().startsWith(QString::fromUtf8(text));
}
project::ProjectSnapshot project_for(const project::TransientDraft& d) {
    auto p = project::default_project();
    p.circuits.transient = d;
    p.circuits.selected_tab = "transient";
    p.selected_domain = "circuits";
    return p;
}
} // namespace
class TransientExecutionTests final : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void analyticalRcChargeDischarge_data() {
        QTest::addColumn<bool>("discharge");
        QTest::newRow("charge") << false;
        QTest::newRow("discharge") << true;
    }
    void analyticalRcChargeDischarge() {
        QFETCH(bool, discharge);
        auto d = rc();
        if (discharge) {
            d.components[0].value.text = "0";
            d.initial_conditions[0].value.text = "10";
        }
        TransientView w(nullptr, &d);
        w.show();
        QSignalSpy edited(&w, &DraftView::draftEdited);
        click(w, "transient_run");
        QTRY_VERIFY_WITH_TIMEOUT(state(w, "Complete"), 10000);
        auto r = w.result();
        QVERIFY(r);
        const double expected = 10 * (discharge ? std::exp(-5.) : 1 - std::exp(-5.));
        QVERIFY(std::abs(r->voltages[0].back() - expected) < .02);
        QVERIFY(discharge ? r->currents[0].back() > 0 : r->currents[0].back() < 0);
        QCOMPARE(edited.count(), 0);
        auto* table = control<QTableView>(w, "transient_trace");
        QCOMPARE(table->model()->columnCount(), 5);
        QVERIFY(
            table->model()->headerData(2, Qt::Horizontal).toString().contains("Supply current"));
        QVERIFY(
            table->model()->headerData(3, Qt::Horizontal).toString().contains("Output <b>π</b>"));
        QCOMPARE(table->model()
                     ->data(table->model()->index(table->model()->rowCount() - 1, 3))
                     .toString()
                     .toDouble(),
                 r->voltages[0].back());
        QCOMPARE(table->model()->headerData(0, Qt::Horizontal).toString(), QString("Time (ms)"));
    }
    void operatingPointAndInactiveConfiguration() {
        auto d = rc();
        d.initialization = "operating_point";
        d.initial_conditions.push_back({project::Id{999}, "inductor_current", {"1e-", "mA"}});
        for (auto& c : d.components)
            c.source.points = {{{"1e-", "ns"}, "NaN"}};
        TransientView w(nullptr, &d);
        click(w, "transient_run");
        QTRY_VERIFY_WITH_TIMEOUT(state(w, "Complete"), 10000);
        QCOMPARE(w.result()->voltages[0].front(), 10.);
        QCOMPARE(w.result()->voltages[0].back(), 10.);
        QCOMPARE(d.components[0].source.points[0].value_text, std::string("NaN"));
        // Inactive edits persist without invalidating this execution.
        control<QTableWidget>(w, "transient_initial_conditions")->item(1, 2)->setText("?");
        QVERIFY(state(w, "Complete"));
    }
    void rlAndRlc_data() {
        QTest::addColumn<double>("resistance");
        QTest::newRow("RL") << -1.;
        QTest::newRow("underdamped") << 20.;
        QTest::newRow("critical") << 200.;
        QTest::newRow("overdamped") << 400.;
    }
    void rlAndRlc() {
        QFETCH(double, resistance);
        auto d = rc();
        d.components[0].value = {"1", "V"};
        if (resistance < 0) {
            d.components[1].value = {"1000", "ohm"};
            d.components[2].kind = "inductor";
            d.components[2].value = {"10", "H"};
            d.initial_conditions = {{project::Id{12}, "inductor_current", {"0", "A"}}};
            d.probes = {{"I", "current", {}, {}, project::Id{12}}};
            d.stop = {"50", "ms"};
            d.maximum_step = {"0.1", "ms"};
        } else {
            d.nodes.push_back({{3}, "L output"});
            d.next_node = {4};
            d.next_component = {14};
            d.components[1].value = {QString::number(resistance).toStdString(), "ohm"};
            d.components[2].positive = project::Id{3};
            d.components.push_back(
                {{13}, "L", "inductor", project::Id{2}, project::Id{3}, {"10", "mH"}, {}});
            d.initial_conditions.push_back({project::Id{13}, "inductor_current", {"0", "A"}});
            d.probes = {{"V_C", "voltage", project::Id{3}, project::Id{0}, {}}};
            d.stop = {"1.5", "ms"};
            d.maximum_step = {"0.1", "us"};
        }
        TransientView w(nullptr, &d);
        click(w, "transient_run");
        QTRY_VERIFY_WITH_TIMEOUT(state(w, "Complete"), 15000);
        const double t = w.result()->current_time_seconds;
        if (resistance < 0)
            QVERIFY(std::abs(w.result()->currents[0].back() - .001 * (1 - std::exp(-t / .01))) <
                    1e-6);
        else {
            const double alpha = resistance / .02, omega = 10000;
            double expected;
            if (alpha < omega) {
                const double b = std::sqrt(omega * omega - alpha * alpha);
                expected =
                    1 - std::exp(-alpha * t) * (std::cos(b * t) + alpha / b * std::sin(b * t));
            } else if (alpha == omega)
                expected = 1 - std::exp(-omega * t) * (1 + omega * t);
            else {
                const double a = -alpha + std::sqrt(alpha * alpha - omega * omega),
                             b = -alpha - std::sqrt(alpha * alpha - omega * omega);
                expected = 1 - (b * std::exp(a * t) - a * std::exp(b * t)) / (b - a);
            }
            QVERIFY(std::abs(w.result()->voltages[0].back() - expected) < .003);
        }
    }
    void runRepeatedStepAndReset() {
        auto d = rc();
        d.maximum_step = {"0.5", "ms"};
        TransientView w(nullptr, &d);
        click(w, "transient_run");
        QTRY_VERIFY_WITH_TIMEOUT(state(w, "Complete"), 10000);
        auto full = w.result();
        click(w, "transient_reset");
        QVERIFY(!w.result());
        QVERIFY(state(w, "Ready"));
        std::size_t rows = 1;
        while (!state(w, "Complete")) {
            click(w, "transient_step_execution");
            ++rows;
            QTRY_VERIFY_WITH_TIMEOUT(w.result() && w.result()->times.size() == rows &&
                                         (state(w, "Paused") || state(w, "Complete")),
                                     5000);
        }
        QCOMPARE(w.result()->voltages, full->voltages);
        QCOMPARE(w.result()->currents, full->currents);
        QCOMPARE(w.result()->times.size(), full->times.size());
        for (std::size_t i = 0; i < full->times.size(); ++i)
            QCOMPARE(w.result()->times[i].seconds, full->times[i].seconds);
    }
    void pauseResumeCancelAndPendingEdit() {
        auto d = rc();
        d.stop = {"0.1", "s"};
        d.maximum_step = {"2", "us"};
        TransientView w(nullptr, &d);
        w.show();
        click(w, "transient_run");
        click(w, "transient_pause");
        QTRY_VERIFY_WITH_TIMEOUT(state(w, "Paused"), 5000);
        auto before = w.result();
        QVERIFY(before);
        QTest::qWait(100);
        QCOMPARE(w.result(), before);
        click(w, "transient_step_execution");
        QTRY_VERIFY_WITH_TIMEOUT(w.result()->times.size() == before->times.size() + 1, 5000);
        click(w, "transient_pause");
        QVERIFY(state(w, "Running"));
        click(w, "transient_pause");
        QTRY_VERIFY_WITH_TIMEOUT(state(w, "Paused"), 5000);
        before = w.result();
        click(w, "transient_cancel");
        QTRY_VERIFY_WITH_TIMEOUT(state(w, "Cancelled"), 5000);
        QCOMPARE(w.result()->times.size(), before->times.size());
        QCOMPARE(w.result()->voltages, before->voltages);
        QVERIFY(!control<QPushButton>(w, "transient_pause")->isEnabled());
        auto* line = control<QLineEdit>(w, "transient_step");
        line->setFocus();
        line->selectAll();
        QTest::keyClicks(line, "1e-");
        QVERIFY(state(w, "Stale"));
        QCOMPARE(d.maximum_step.text, std::string("1e-"));
        const auto labels = w.result_probes();
        control<QTableWidget>(w, "transient_probes")->item(1, 0)->setText("different");
        auto* reference = qobject_cast<QComboBox*>(
            control<QTableWidget>(w, "transient_probes")->cellWidget(1, 2));
        QVERIFY(reference);
        reference->setCurrentIndex(reference->findData(1U));
        QCOMPARE(d.probes[1].positive, project::Reference{project::Id{1}});
        QCOMPARE(w.result_probes()[1].label, labels[1].label);
        QCOMPARE(std::get<tr::VoltageProbe>(w.result_probes()[1].reference).positive.value, 2U);
        QCOMPARE(std::get<tr::CurrentProbe>(w.result_probes()[0].reference).component.value, 10U);
        QVERIFY(w.result());
        click(w, "transient_run");
        QVERIFY(state(w, "Failed"));
        QVERIFY(!w.result());
    }
    void rawDelegateInvalidatesImmediately() {
        auto d = rc();
        TransientView w(nullptr, &d);
        w.show();
        click(w, "transient_run");
        QTRY_VERIFY_WITH_TIMEOUT(state(w, "Complete"), 5000);
        auto* table = control<QTableWidget>(w, "transient_components");
        table->setCurrentCell(1, 5);
        table->editItem(table->item(1, 5));
        auto* line = table->findChild<QLineEdit*>();
        QVERIFY(line);
        line->selectAll();
        QTest::keyClicks(line, "1e-");
        QVERIFY(state(w, "Stale"));
        w.synchronize_pending_text();
        QCOMPARE(d.components[1].value.text, std::string("1e-"));
    }
    void breakpointPlotAndTable_data() {
        QTest::addColumn<bool>("linear");
        QTest::newRow("hold") << false;
        QTest::newRow("linear") << true;
    }
    void breakpointPlotAndTable() {
        QFETCH(bool, linear);
        auto d = rc();
        d.components[0].source = {linear ? "linear" : "hold",
                                  {{{"1", "ms"}, "5"}, {{"2", "ms"}, "10"}}};
        d.probes.push_back({"Supply", "voltage", project::Id{1}, project::Id{0}, {}});
        TransientView w(nullptr, &d);
        click(w, "transient_run");
        QTRY_VERIFY_WITH_TIMEOUT(state(w, "Complete"), 5000);
        auto r = w.result();
        const auto indices = transient_plot_indices(*r, r->voltages[1]);
        control<QComboBox>(w, "transient_voltage_trace")->setCurrentIndex(1);
        auto* plot = control<QwtPlot>(w, "transient_voltage_plot");
        const auto items = plot->itemList(QwtPlotItem::Rtti_PlotCurve);
        QCOMPARE(items.size(), 1);
        auto* curve = static_cast<QwtPlotCurve*>(items.front());
        QCOMPARE(curve->dataSize(), indices.size());
        for (std::size_t j = 0; j < indices.size(); ++j) {
            QCOMPARE(curve->sample(static_cast<int>(j)).x(), r->times[indices[j]].seconds / .001);
            QCOMPARE(curve->sample(static_cast<int>(j)).y(), r->voltages[1][indices[j]]);
        }

        for (auto t : {.001, .002}) {
            auto it = std::find_if(r->times.begin(), r->times.end(),
                                   [&](auto p) { return p.seconds == t; });
            QVERIFY(it != r->times.end());
            const auto i = static_cast<std::size_t>(it - r->times.begin());
            QCOMPARE(r->times[i].side, tr::SampleSide::before_breakpoint);
            QCOMPARE(r->times[i + 1].side, tr::SampleSide::after_breakpoint);
            QCOMPARE(r->times[i + 1].seconds, t);
            QVERIFY(std::ranges::find(indices, i) != indices.end());
            QVERIFY(std::ranges::find(indices, i + 1) != indices.end());
            QCOMPARE(r->voltages[0][i], r->voltages[0][i + 1]);
            if (linear)
                QCOMPARE(r->voltages[1][i], r->voltages[1][i + 1]);
            else
                QVERIFY(r->voltages[1][i] != r->voltages[1][i + 1]);
        }
    }
    void rejectedDraftBaselineAndEmptyTraceStates() {
        auto d = rc();
        d.initialization = "operating_point";
        d.stop.text = "1e-";
        TransientView w(nullptr, &d);
        click(w, "transient_run");
        QVERIFY(state(w, "Failed"));
        QVERIFY(!w.result());
        // An inactive edit must not compare against some older successful run.
        control<QTableWidget>(w, "transient_initial_conditions")
            ->item(0, 2)
            ->setText("disabled invalid");
        QVERIFY(state(w, "Failed"));
        control<QLineEdit>(w, "transient_stop")->setText("5");
        QVERIFY(state(w, "Stale"));
        QVERIFY(control<QLabel>(w, "transient_status")->text().contains("no accepted trace"));
        click(w, "transient_run");
        QVERIFY(state(w, "Running"));
        QVERIFY(control<QLabel>(w, "transient_status")->text().contains("initializing"));
        QTRY_VERIFY_WITH_TIMEOUT(state(w, "Complete"), 5000);
    }
    void initializationAndIntegrationFailure() {
        auto d = rc();
        d.nodes.push_back({{5}, "Floating <b>π</b>"});
        d.next_node = {6};
        TransientView failed(nullptr, &d);
        click(failed, "transient_run");
        QTRY_VERIFY_WITH_TIMEOUT(state(failed, "Failed"), 5000);
        QVERIFY(!failed.result());
        QCOMPARE(control<QLabel>(failed, "transient_diagnostic")->textFormat(), Qt::PlainText);
        d = rc();
        d.nodes.resize(2);
        d.components.erase(d.components.begin() + 1);
        d.components[1].positive = project::Id{1};
        d.components[0].value.text = "0";
        d.components[0].source = {"hold", {{{"50", "ms"}, "1"}}};
        d.stop = {"100", "ms"};
        d.maximum_step = {"40", "ms"};
        d.probes = {{"V", "voltage", project::Id{1}, project::Id{0}, {}}};
        TransientView integration(nullptr, &d);
        click(integration, "transient_run");
        QTRY_VERIFY_WITH_TIMEOUT(state(integration, "Failed"), 5000);
        QVERIFY(integration.result());
        QVERIFY(integration.result()->failure);
        QCOMPARE(integration.result()->failure->time_seconds, .05);
        QVERIFY(integration.result()->current_time_seconds < .05);
        QVERIFY(control<QLabel>(integration, "transient_diagnostic")
                    ->text()
                    .contains("last accepted time"));
        click(integration, "transient_cancel");
        QVERIFY(integration.result()->failure);
    }
    void documentReplacementAndInertRoundTrip() {
        auto d = rc();
        d.stop = {"0.1", "s"};
        d.maximum_step = {"2", "us"};
        auto p = project_for(d);
        ProjectDocument doc(std::make_unique<ProjectWorkspace>(p, true));
        auto* view = control<TransientView>(doc.workspace(), "transient_view");
        click(*view, "transient_run");
        click(*view, "transient_pause");
        QTRY_VERIFY_WITH_TIMEOUT(state(*view, "Paused"), 5000);
        QVERIFY(!doc.dirty());
        QCOMPARE(doc.workspace().capture(), p);
        QTemporaryDir dir;
        auto path = dir.filePath("Transient π saved.openece");
        QVERIFY(std::holds_alternative<SaveStatus>(doc.save_as(path)));
        auto candidate = doc.prepare_open(path);
        QVERIFY(std::holds_alternative<PreparedProject>(candidate));
        QVERIFY(std::holds_alternative<std::monostate>(
            doc.install(std::get<PreparedProject>(std::move(candidate)))));
        view = control<TransientView>(doc.workspace(), "transient_view");
        QVERIFY(!view->result());
        QVERIFY(state(*view, "Ready"));
        QCOMPARE(doc.workspace().capture(), p);
        click(*view, "transient_run");
        QVERIFY(state(*view, "Running"));
        auto fresh = doc.prepare_new(project::default_project(), DocumentState::clean);
        QVERIFY(std::holds_alternative<PreparedProject>(fresh));
        QVERIFY(std::holds_alternative<std::monostate>(
            doc.install(std::get<PreparedProject>(std::move(fresh)))));
        view = control<TransientView>(doc.workspace(), "transient_view");
        QVERIFY(!view->result());
    }
    void closeStopsActiveWorker() {
        auto d = rc();
        d.stop = {"0.1", "s"};
        d.maximum_step = {"2", "us"};
        ProjectWorkspace w(project_for(d), true);
        auto* view = control<TransientView>(w, "transient_view");
        click(*view, "transient_run");
        QVERIFY(state(*view, "Running"));
        w.stop_execution();
        QVERIFY(state(*view, "Cancelled"));
        QVERIFY(view->result());
    }
    void boundedWorkloadRemainsResponsive_data() {
        QTest::addColumn<bool>("dense");
        QTest::newRow("32-node-dense-work") << true;
        QTest::newRow("1.6-million-probe-values") << false;
    }
    void boundedWorkloadRemainsResponsive() {
        QFETCH(bool, dense);
        auto d = rc();
        d.nodes.clear();
        d.components.clear();
        d.initial_conditions.clear();
        d.probes.clear();
        d.initialization = "operating_point";
        d.nodes.push_back({{0}, "g"});
        d.ground = project::Id{0};
        d.next_node = {32};
        d.next_component = {1001};
        for (std::uint32_t i = 1; i < 32; ++i) {
            d.nodes.push_back({{i}, "N" + std::to_string(i)});
            d.components.push_back({{2 * i},
                                    "R" + std::to_string(i),
                                    "resistor",
                                    project::Id{i},
                                    project::Id{0},
                                    {"1000", "ohm"},
                                    {}});
            d.components.push_back({{2 * i + 1},
                                    "C" + std::to_string(i),
                                    "capacitor",
                                    project::Id{i},
                                    project::Id{0},
                                    {"1", "uF"},
                                    {}});
        }
        d.components.push_back(
            {{1000}, "I", "current_source", project::Id{0}, project::Id{1}, {"1", "mA"}, {}});
        for (int i = 0; i < 64; ++i)
            d.probes.push_back(
                {"V" + std::to_string(i), "voltage", project::Id{1}, project::Id{0}, {}});
        d.stop = {"1", "s"};
        d.maximum_step = {"1", "ms"};
        if (!dense) {
            d = rc();
            d.stop = {"0.1", "s"};
            d.maximum_step = {"4", "us"};
            d.probes.clear();
            for (int i = 0; i < 64; ++i)
                d.probes.push_back(
                    {"V" + std::to_string(i), "voltage", project::Id{2}, project::Id{0}, {}});
        }
        TransientView w(nullptr, &d);
        w.show();
        QElapsedTimer clock;
        clock.start();
        qint64 last = 0, gap = 0;
        int ticks = 0;
        QTimer heartbeat;
        connect(&heartbeat, &QTimer::timeout, this, [&] {
            auto now = clock.elapsed();
            gap = std::max(gap, now - last);
            last = now;
            ++ticks;
        });
        heartbeat.start(10);
        click(w, "transient_run");
        QTRY_VERIFY_WITH_TIMEOUT(state(w, "Complete"), 90000);
        heartbeat.stop();
        QVERIFY(ticks > 3);
        QVERIFY2(gap < 1000, "GUI event loop blocked for a second");
        QVERIFY(w.result()->times.size() >= (dense ? 1001U : 25001U));
        qInfo() << "Transient dense=" << dense << " / 64 probes: elapsed_ms=" << clock.elapsed()
                << " heartbeat_max_gap_ms=" << gap << " rows=" << w.result()->times.size();
    }
    void fullTraceAndDecimationBudgets() {
        auto r = std::make_shared<tr::Result>();
        r->times.resize(31250);
        r->voltages.resize(64, std::vector<double>(31250, 1));
        for (std::size_t i = 0; i < r->times.size(); ++i)
            r->times[i] = {double(i), tr::SampleSide::regular};
        r->times[10000] = {10000, tr::SampleSide::before_breakpoint};
        r->times[10001] = {10000, tr::SampleSide::after_breakpoint};
        r->voltages[0][10000] = -50;
        r->voltages[0][10001] = 50;
        const auto indices = transient_plot_indices(*r, r->voltages[0]);
        QVERIFY(indices.size() <= 4 * transient_plot_buckets + 4);
        QVERIFY(std::ranges::find(indices, 10000) != indices.end());
        QVERIFY(std::ranges::find(indices, 10001) != indices.end());
        TransientTraceModel m(nullptr);
        m.set_result(r, {{"V", true, 0, tr::VoltageProbe{{1}, {0}}}}, "s", 1);
        QCOMPARE(m.rowCount(), 31250);
        QCOMPARE(m.data(m.index(10001, 2)).toString(), QString("50"));
    }
};
QTEST_MAIN(TransientExecutionTests)
#include "transient_execution_gui_test.moc"
