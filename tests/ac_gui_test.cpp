#include "ac_response_plot.hpp"
#include "ac_view.hpp"
#include "main_window.hpp"
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QtTest>
#include <cmath>
#include <numbers>
#include <qwt_plot_curve.h>
#include <qwt_text.h>
using namespace openece::gui;
class AcGuiTest : public QObject {
    Q_OBJECT
    QTableWidget* table(QWidget& v, const char* name) { return v.findChild<QTableWidget*>(name); }
    QTableWidget* parts(QWidget& v) { return table(v, "ac_components"); }
    QComboBox* select(QWidget& v, int row, int col) {
        return static_cast<QComboBox*>(parts(v)->cellWidget(row, col));
    }
    QLineEdit* value(QWidget& v, int row, int col = 5) {
        return static_cast<QLineEdit*>(parts(v)->cellWidget(row, col));
    }
    QComboBox* selector(QWidget& v, const char* name) { return v.findChild<QComboBox*>(name); }
    void node(QWidget& v, int row, int col, unsigned id) {
        auto* c = select(v, row, col);
        c->setCurrentIndex(c->findData(id));
    }
    void choose(QWidget& v, const char* name, unsigned id) {
        auto* c = selector(v, name);
        c->setCurrentIndex(c->findData(id));
    }
    void click(QWidget& v, const char* name) {
        v.findChild<QPushButton*>(name)->click();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    QString status(QWidget& v) { return v.findChild<QLabel*>("ac_status")->text(); }
    void text(QWidget& v, const char* name, const QString& text) {
        v.findChild<QLineEdit*>(name)->setText(text);
    }
    void points(QWidget& v, int count) {
        v.findChild<QSpinBox*>("ac_point_count")->setValue(count);
    }
  private Q_SLOTS:
    void rcCornerSingleFrequencyAndSourcePhase() {
        AcView v;
        QVERIFY(status(v).startsWith("Solved AC"));
        auto* volts = table(v, "ac_voltages");
        QCOMPARE(volts->rowCount(), 3);
        QCOMPARE(volts->item(2, 1)->text().toDouble(), .5);
        QCOMPARE(volts->item(2, 2)->text().toDouble(), -.5);
        QVERIFY(std::abs(volts->item(2, 3)->text().toDouble() - std::sqrt(.5)) < 1e-11);
        QCOMPARE(volts->item(2, 4)->text().toDouble(), -45.);
        QCOMPARE(volts->item(0, 4)->text(), QString::fromUtf8("—"));
        QCOMPARE(table(v, "ac_currents")->item(0, 1)->text().toDouble(), -.0005);
        value(v, 0, 7)->setText("90");
        QCOMPARE(volts->rowCount(), 0);
        click(v, "ac_solve");
        QCOMPARE(volts->item(2, 4)->text().toDouble(), 45.);
        value(v, 0, 7)->setText("180");
        click(v, "ac_solve");
        QCOMPARE(volts->item(1, 4)->text().toDouble(), 180.);
        value(v, 0, 7)->setText("invalid");
        click(v, "ac_solve");
        QVERIFY(status(v).contains("phase"));
        QCOMPARE(volts->rowCount(), 0);
    }
    void unitsConvertAndInvalidPendingTextSurvives() {
        AcView v;
        QCOMPARE(value(v, 1)->text(), QString("1"));
        QCOMPARE(select(v, 1, 6)->currentText(), QString::fromUtf8("kΩ"));
        select(v, 1, 6)->setCurrentIndex(0);
        QCOMPARE(value(v, 1)->text(), QString("1000"));
        select(v, 2, 6)->setCurrentIndex(2);
        QVERIFY(std::abs(value(v, 2)->text().toDouble() - 1000) < 1e-9);
        const double original = v.findChild<QLineEdit*>("ac_frequency")->text().toDouble();
        selector(v, "ac_frequency_unit")->setCurrentIndex(1);
        QVERIFY(std::abs(v.findChild<QLineEdit*>("ac_frequency")->text().toDouble() * 1000 -
                         original) < 1e-12);
        click(v, "ac_solve");
        QCOMPARE(table(v, "ac_voltages")->item(2, 4)->text().toDouble(), -45.);
        value(v, 2)->setText("1e-");
        select(v, 2, 6)->setCurrentIndex(1);
        QCOMPARE(select(v, 2, 6)->currentIndex(), 2);
        QCOMPARE(value(v, 2)->text(), QString("1e-"));
        QVERIFY(status(v).contains("Invalid pending"));
        text(v, "ac_frequency", "1e-");
        selector(v, "ac_frequency_unit")->setCurrentIndex(0);
        QCOMPARE(selector(v, "ac_frequency_unit")->currentIndex(), 1);
        QCOMPARE(v.findChild<QLineEdit*>("ac_frequency")->text(), QString("1e-"));
        click(v, "ac_solve");
        QVERIFY(status(v).contains("Invalid numeric"));
        click(v, "ac_example_rc");
        QVERIFY(status(v).startsWith("Solved AC"));
        text(v, "ac_frequency", "0");
        click(v, "ac_solve");
        QVERIFY(status(v).contains("frequency"));
    }
    void transferAndAbsoluteResponsesAreClearlyDifferent() {
        AcView v;
        value(v, 0)->setText("5");
        value(v, 0, 7)->setText("30");
        const double corner = 1 / (2 * std::numbers::pi * 1000 * 1e-6);
        text(v, "ac_start", QString::number(corner, 'g', 17));
        text(v, "ac_stop", QString::number(corner * 10, 'g', 17));
        points(v, 3);
        click(v, "ac_run_sweep");
        QTRY_VERIFY(status(v).startsWith("Sweep complete"));
        auto* result = table(v, "ac_sweep_results");
        QCOMPARE(result->rowCount(), 3);
        QVERIFY(std::abs(result->item(0, 3)->text().toDouble() + 3.01029995664) < 1e-9);
        QCOMPARE(result->item(0, 4)->text().toDouble(), -45.);
        QVERIFY(result->horizontalHeaderItem(3)->text().contains("dB"));
        auto* plot = v.findChild<QwtPlot*>("ac_magnitude_plot");
        QVERIFY(plot->axisTitle(QwtPlot::yLeft).text().contains("dB"));
        selector(v, "ac_response_mode")->setCurrentIndex(1);
        click(v, "ac_run_sweep");
        QTRY_VERIFY(status(v).startsWith("Sweep complete"));
        QVERIFY(std::abs(result->item(0, 3)->text().toDouble() - 5 / std::sqrt(2.)) < 1e-10);
        QCOMPARE(result->item(0, 4)->text().toDouble(), -15.);
        QVERIFY(result->horizontalHeaderItem(3)->text().contains("V RMS"));
        QVERIFY(plot->axisTitle(QwtPlot::yLeft).text().contains("V RMS"));
        QCOMPARE(value(v, 0)->text(), QString("5"));
        QCOMPARE(value(v, 0, 7)->text(), QString("30"));
    }
    void nonzeroOtherSourcesAreRejectedWithoutMutation() {
        AcView v;
        click(v, "ac_add_component");
        select(v, 3, 2)->setCurrentIndex(4);
        value(v, 3)->setText(".001");
        node(v, 3, 3, 0);
        node(v, 3, 4, 2);
        click(v, "ac_run_sweep");
        QVERIFY(status(v).contains("every other"));
        QCOMPARE(value(v, 3)->text(), QString(".001"));
        QCOMPARE(table(v, "ac_sweep_results")->rowCount(), 0);
        selector(v, "ac_response_mode")->setCurrentIndex(1);
        points(v, 2);
        click(v, "ac_run_sweep");
        QTRY_VERIFY(status(v).startsWith("Sweep complete"));
        value(v, 3)->setText("0");
        selector(v, "ac_response_mode")->setCurrentIndex(0);
        click(v, "ac_run_sweep");
        QTRY_VERIFY(status(v).startsWith("Sweep complete"));
        value(v, 0)->setText("0");
        click(v, "ac_run_sweep");
        QVERIFY(status(v).contains("nonzero"));
        click(v, "ac_example_rc");
        select(v, 1, 2)->setCurrentIndex(3);
        value(v, 1)->setText(".1");
        click(v, "ac_run_sweep");
        QVERIFY(status(v).contains("every other"));
        QCOMPARE(value(v, 1)->text(), QString(".1"));
    }
    void missingReferencesAndInvalidDraftRecovery() {
        AcView v;
        table(v, "ac_nodes")->selectRow(2);
        click(v, "ac_remove_node");
        QVERIFY(select(v, 1, 4)->currentText().contains("Missing node 2"));
        QVERIFY(selector(v, "ac_probe_positive")->currentText().contains("Missing node 2"));
        click(v, "ac_add_node");
        QCOMPARE(table(v, "ac_nodes")->item(2, 0)->text(), QString("3"));
        click(v, "ac_solve");
        QVERIFY(status(v).contains("existing terminals"));
        node(v, 1, 4, 3);
        node(v, 2, 3, 3);
        click(v, "ac_solve");
        QVERIFY(status(v).startsWith("Solved AC"));
        click(v, "ac_run_sweep");
        QVERIFY(status(v).contains("missing node"));
        choose(v, "ac_probe_positive", 3);
        points(v, 2);
        click(v, "ac_run_sweep");
        QTRY_VERIFY(status(v).startsWith("Sweep complete"));
        table(v, "ac_nodes")->item(2, 1)->setText(QString::fromUtf8("π output"));
        QCOMPARE(table(v, "ac_sweep_results")->rowCount(), 0);
        QVERIFY(selector(v, "ac_probe_positive")
                    ->currentText()
                    .contains(QString::fromUtf8("π output")));
        click(v, "ac_add_component");
        value(v, 3)->setText("1000");
        click(v, "ac_solve");
        QVERIFY(status(v).contains("Select a node"));
        node(v, 3, 3, 3);
        node(v, 3, 4, 0);
        click(v, "ac_solve");
        QVERIFY(status(v).startsWith("Solved AC"));
        selector(v, "ac_ground")->setCurrentIndex(0);
        click(v, "ac_solve");
        QVERIFY(status(v).contains("ground"));
    }
    void singularFrequencyRetainsItsRowAndCreatesPlotGaps() {
        AcView v;
        const QString reactive = QString::number(1 / (2 * std::numbers::pi), 'g', 17);
        select(v, 0, 2)->setCurrentIndex(4);
        value(v, 0)->setText("1");
        node(v, 0, 3, 0);
        node(v, 0, 4, 1);
        select(v, 1, 2)->setCurrentIndex(1);
        value(v, 1)->setText(reactive);
        node(v, 1, 3, 1);
        node(v, 1, 4, 0);
        select(v, 2, 2)->setCurrentIndex(2);
        value(v, 2)->setText(reactive);
        node(v, 2, 3, 1);
        node(v, 2, 4, 0);
        table(v, "ac_nodes")->selectRow(2);
        click(v, "ac_remove_node");
        choose(v, "ac_probe_positive", 1);
        selector(v, "ac_response_mode")->setCurrentIndex(1);
        selector(v, "ac_spacing")->setCurrentIndex(1);
        text(v, "ac_start", ".5");
        text(v, "ac_stop", "1.5");
        points(v, 3);
        click(v, "ac_run_sweep");
        QTRY_VERIFY(status(v).startsWith("Sweep complete"));
        QVERIFY(status(v).contains("1 failures"));
        auto* result = table(v, "ac_sweep_results");
        QCOMPARE(result->rowCount(), 3);
        QCOMPARE(result->item(1, 0)->data(Qt::UserRole).toDouble(), 1.);
        QCOMPARE(result->item(1, 3)->text(), QString::fromUtf8("—"));
        QCOMPARE(result->item(1, 5)->data(Qt::UserRole).toInt(),
                 static_cast<int>(openece::circuits::ErrorCode::rank_deficient));
        QCOMPARE(result->item(0, 5)->text(), QString("Accepted"));
        QCOMPARE(result->item(2, 5)->text(), QString("Accepted"));
        for (const char* name : {"ac_magnitude_plot", "ac_phase_plot"}) {
            const auto curves = v.findChild<QwtPlot*>(name)->itemList(QwtPlotItem::Rtti_PlotCurve);
            QCOMPARE(curves.size(), 2);
            for (auto* item : curves)
                QCOMPARE(static_cast<QwtPlotCurve*>(item)->dataSize(), std::size_t{1});
        }
    }
    void cancellationPreservesRequestedFrequenciesAndEditsInvalidate() {
        AcView v;
        points(v, ac_gui_limits::sweep_points);
        click(v, "ac_run_sweep");
        auto* result = table(v, "ac_sweep_results");
        QCOMPARE(result->rowCount(), ac_gui_limits::sweep_points);
        QTimer::singleShot(0, &v, [&] { click(v, "ac_cancel"); });
        QTRY_VERIFY(status(v).startsWith("Sweep cancelled"));
        QCOMPARE(result->rowCount(), ac_gui_limits::sweep_points);
        QVERIFY(result->item(result->rowCount() - 1, 5)->text().contains("Not evaluated"));
        const QString cancelled = status(v);
        QTest::qWait(10);
        QCOMPARE(status(v), cancelled);
        click(v, "ac_run_sweep");
        value(v, 0)->setText("2");
        QCOMPARE(result->rowCount(), 0);
        QTest::qWait(10);
        QCOMPARE(result->rowCount(), 0);
        QVERIFY(status(v).startsWith("Draft changed"));
        points(v, 3);
        click(v, "ac_run_sweep");
        QTRY_VERIFY(status(v).startsWith("Sweep complete"));
        QCOMPARE(result->rowCount(), 3);
        auto active = std::make_unique<AcView>();
        points(*active, 1001);
        click(*active, "ac_run_sweep");
        active.reset();
        QTest::qWait(1);
    }
    void zeroGainHasFloorAndUndefinedPhase() {
        AcView v;
        choose(v, "ac_probe_positive", 0);
        points(v, 2);
        click(v, "ac_run_sweep");
        QTRY_VERIFY(status(v).startsWith("Sweep complete"));
        auto* result = table(v, "ac_sweep_results");
        QCOMPARE(result->item(0, 3)->text().toDouble(), -240.);
        QCOMPARE(result->item(0, 4)->text(), QString::fromUtf8("—"));
        QCOMPARE(
            v.findChild<QwtPlot*>("ac_phase_plot")->itemList(QwtPlotItem::Rtti_PlotCurve).size(),
            0);
        selector(v, "ac_response_mode")->setCurrentIndex(1);
        click(v, "ac_run_sweep");
        QTRY_VERIFY(status(v).startsWith("Sweep complete"));
        QCOMPARE(result->item(0, 3)->text().toDouble(), 0.);
    }
    void plotAdapterSplitsGapsAndWrappedPhaseDiscontinuities() {
        AcResponsePlot phase(true);
        const std::vector<double> f{1, 2, 3, 4, 5};
        const std::vector<std::optional<double>> values{170, -170, std::nullopt, 0, 1};
        phase.set_response(f, values, true, "Phase (degrees)");
        const auto curves = phase.itemList(QwtPlotItem::Rtti_PlotCurve);
        QCOMPARE(curves.size(), 3);
        QCOMPARE(static_cast<QwtPlotCurve*>(curves[0])->dataSize(), std::size_t{1});
        QCOMPARE(static_cast<QwtPlotCurve*>(curves[1])->dataSize(), std::size_t{1});
        QCOMPARE(static_cast<QwtPlotCurve*>(curves[2])->dataSize(), std::size_t{2});
        phase.clear();
        QCOMPARE(phase.itemList(QwtPlotItem::Rtti_PlotCurve).size(), 0);
    }
    void domainAndDcAcPersistenceWithRlcExample() {
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* nav = window.findChild<QListWidget*>("domain_navigation");
        QCOMPARE(nav->count(), 4);
        nav->setCurrentRow(2);
        auto* tabs = window.findChild<QTabWidget*>("circuits_analysis_tabs");
        QCOMPARE(tabs->count(), 2);
        auto* dc = table(window, "circuit_components");
        static_cast<QLineEdit*>(dc->cellWidget(0, 5))->setText("12");
        click(window, "circuit_solve");
        QCOMPARE(table(window, "circuit_voltages")->item(2, 1)->text().toDouble(), 6.);
        tabs->setCurrentIndex(1);
        value(window, 0)->setText("3");
        click(window, "ac_solve");
        QCOMPARE(table(window, "ac_voltages")->item(2, 1)->text().toDouble(), 1.5);
        nav->setCurrentRow(0);
        nav->setCurrentRow(1);
        nav->setCurrentRow(2);
        tabs->setCurrentIndex(0);
        QCOMPARE(table(window, "circuit_voltages")->item(2, 1)->text().toDouble(), 6.);
        tabs->setCurrentIndex(1);
        QCOMPARE(value(window, 0)->text(), QString("3"));
        click(window, "ac_example_rlc");
        QVERIFY(status(window).startsWith("Solved AC"));
        QVERIFY(std::abs(table(window, "ac_voltages")->item(3, 3)->text().toDouble() - 1) < 1e-10);
        points(window, 41);
        click(window, "ac_run_sweep");
        QTRY_VERIFY(status(window).startsWith("Sweep complete"));
        const auto path = qEnvironmentVariable("OPENECE_AC_SCREENSHOT");
        if (!path.isEmpty())
            QVERIFY(window.grab().save(path));
    }
    void guiLimitsAndLocaleIndependentInput() {
        AcView v;
        auto* count = v.findChild<QSpinBox*>("ac_point_count");
        count->setValue(1002);
        QCOMPARE(count->value(), 1001);
        for (int i = 3; i <= ac_gui_limits::nodes; ++i)
            click(v, "ac_add_node");
        QCOMPARE(table(v, "ac_nodes")->rowCount(), ac_gui_limits::nodes);
        QVERIFY(status(v).contains("limit"));
        for (int i = 3; i <= ac_gui_limits::components; ++i)
            click(v, "ac_add_component");
        QCOMPARE(parts(v)->rowCount(), ac_gui_limits::components);
        QVERIFY(status(v).contains("limit"));
        click(v, "ac_example_rc");
        struct Restore {
            QLocale value;
            ~Restore() { QLocale::setDefault(value); }
        } restore{QLocale()};
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
        value(v, 0)->setText("1,5");
        click(v, "ac_solve");
        QVERIFY(status(v).contains("Invalid numeric"));
        value(v, 0)->setText("1.5");
        click(v, "ac_solve");
        QVERIFY(status(v).startsWith("Solved AC"));
    }
};
QTEST_MAIN(AcGuiTest)
#include "ac_gui_test.moc"
