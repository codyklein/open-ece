#include "main_window.hpp"
#include "timing_view.hpp"
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QtTest>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>

using namespace openece::gui;
namespace d = openece::digital;
class TimingGuiTest : public QObject {
    Q_OBJECT
    template <class T> T* widget(QWidget& view, const char* name) {
        return view.findChild<T*>(name);
    }
    void click(QWidget& view, const char* name) {
        widget<QPushButton>(view, name)->click();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    QTableWidget* table(QWidget& view, const char* name) {
        return widget<QTableWidget>(view, name);
    }
    QString status(QWidget& view) { return widget<QLabel>(view, "timing_status")->text(); }
    QList<QwtPlotItem*> curves(QWidget& view) {
        return widget<QwtPlot>(view, "timing_diagram")->itemList(QwtPlotItem::Rtti_PlotCurve);
    }
    void finish(QWidget& view) {
        click(view, "timing_run");
        QTRY_VERIFY_WITH_TIMEOUT(status(view).startsWith("Complete"), 5000);
    }
  private Q_SLOTS:
    void exampleTracesAndDisplayUnits() {
        MainWindow view;
        view.resize(1280, 950);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        widget<QListWidget>(view, "domain_navigation")->setCurrentRow(1);
        widget<QTabWidget>(view, "digital_tabs")->setCurrentIndex(1);
        finish(view);
        QCOMPARE(table(view, "timing_results")->item(0, 1)->text(), QString("0"));
        QCOMPARE(curves(view).size(), 3);
        auto* q = static_cast<QwtPlotCurve*>(curves(view)[2]);
        QCOMPARE(q->dataSize(), 6U);
        QCOMPARE(q->sample(1), QPointF(6, 0));
        QCOMPARE(q->sample(2), QPointF(6, 0.7));
        QCOMPARE(q->sample(3), QPointF(16, 0.7));
        QCOMPARE(q->sample(4), QPointF(16, 0));
        QCOMPARE(q->sample(5), QPointF(30, 0));
        const auto screenshot = qEnvironmentVariable("OPENECE_TIMING_SCREENSHOT");
        if (!screenshot.isEmpty())
            QVERIFY(view.grab().save(screenshot));
        widget<QComboBox>(view, "timing_scale")->setCurrentText("ps");
        q = static_cast<QwtPlotCurve*>(curves(view)[2]);
        QCOMPARE(q->sample(2), QPointF(6000, 0.7));
        widget<QListWidget>(view, "domain_navigation")->setCurrentRow(0);
        widget<QListWidget>(view, "domain_navigation")->setCurrentRow(1);
        QCOMPARE(widget<QTabWidget>(view, "digital_tabs")->currentIndex(), 1);
        QVERIFY(status(view).startsWith("Complete"));
    }
    void stepPauseResetAndEditingInvalidation() {
        TimingView view;
        click(view, "timing_step");
        QVERIFY(status(view).contains("Paused at 2000 ps"));
        click(view, "timing_step");
        QVERIFY(status(view).contains("Paused at 5000 ps"));
        QCOMPARE(table(view, "timing_results")->item(0, 1)->text(), QString("0"));
        click(view, "timing_step");
        QVERIFY(status(view).contains("Paused at 6000 ps"));
        QCOMPARE(table(view, "timing_results")->item(0, 1)->text(), QString("1"));
        click(view, "timing_run");
        click(view, "timing_pause");
        const auto paused = status(view);
        QTest::qWait(10);
        QCOMPARE(status(view), paused);
        table(view, "timing_stimuli")->item(0, 0)->setText("3000");
        QCOMPARE(curves(view).size(), 0);
        QCOMPARE(table(view, "timing_results")->rowCount(), 0);
        finish(view);
        click(view, "timing_reset");
        QCOMPARE(curves(view).size(), 0);
        click(view, "timing_step");
        QVERIFY(status(view).contains("3000 ps"));
    }
    void invalidDraftTerminalErrorAndRecovery() {
        TimingView view;
        auto* elements = table(view, "timing_elements");
        elements->item(0, 2)->setText("1,999");
        click(view, "timing_step");
        QVERIFY(status(view).contains("Missing source node 999"));
        QCOMPARE(curves(view).size(), 0);
        elements->item(0, 2)->setText("1,2");
        elements->item(0, 3)->setText("0");
        click(view, "timing_step");
        QVERIFY(status(view).contains("positive"));
        click(view, "timing_example");
        static_cast<QComboBox*>(elements->cellWidget(0, 1))->setCurrentText("SR latch");
        click(view, "timing_run");
        QTRY_VERIFY(status(view).contains("Forbidden SR latch"));
        QVERIFY(status(view).contains("5000 ps"));
        QCOMPARE(curves(view).size(), 0);
        QCOMPARE(table(view, "timing_results")->rowCount(), 0);
        click(view, "timing_example");
        finish(view);
        widget<QLineEdit>(view, "timing_observed")->setText("3,3");
        click(view, "timing_step");
        QVERIFY(status(view).contains("Duplicate observed"));
        widget<QLineEdit>(view, "timing_observed")->setText("3");
        widget<QLineEdit>(view, "timing_horizon")->setText("1,000");
        click(view, "timing_step");
        QVERIFY(status(view).contains("decimal integer"));
    }
    void copyIsValidatedIndependentAndPreservesMissingReferences() {
        MainWindow view;
        auto* digital_inputs = table(view, "digital_inputs");
        digital_inputs->item(0, 1)->setText("");
        click(view, "digital_copy_timing");
        QVERIFY(widget<QLabel>(view, "digital_copy_status")->text().startsWith("Cannot copy"));
        QCOMPARE(table(view, "timing_elements")->rowCount(), 1); // Existing timing draft retained.
        digital_inputs->item(0, 1)->setText(" π ");
        click(view, "digital_copy_timing");
        QCOMPARE(table(view, "timing_inputs")->item(0, 1)->text(), QString(" π "));
        QCOMPARE(table(view, "timing_elements")->rowCount(), 2);
        QCOMPARE(table(view, "timing_elements")->item(0, 3)->text(), QString("1000"));
        table(view, "timing_inputs")->selectRow(0);
        click(view, "timing_inputs_remove");
        click(view, "timing_inputs_add");
        QCOMPARE(table(view, "timing_inputs")->item(1, 0)->text(), QString("5"));
        table(view, "timing_inputs")->item(1, 1)->setText("C");
        table(view, "timing_inputs")->item(1, 2)->setText("0");
        click(view, "timing_step");
        QVERIFY(status(view).contains("Missing source node 1"));
        QCOMPARE(digital_inputs->rowCount(), 2);
        QCOMPARE(digital_inputs->item(0, 1)->text(), QString(" π "));
    }
    void combinationalInertialPulseAndLimits() {
        TimingView view;
        const d::Circuit circuit({{{{1}, "A"}}, {{{2}, d::GateKind::And, {{1}}}}, {{"Y", {2}}}});
        view.load_combinational(circuit, {d::LogicValue::zero}, {5});
        widget<QLineEdit>(view, "timing_horizon")->setText("20");
        auto* stimuli = table(view, "timing_stimuli");
        for (int row = 0; row < 2; ++row) {
            click(view, "timing_stimuli_add");
            stimuli->item(row, 0)->setText(row ? "5" : "1");
            stimuli->item(row, 1)->setText("1");
            stimuli->item(row, 2)->setText(row ? "0" : "1");
        }
        finish(view);
        QCOMPARE(static_cast<QwtPlotCurve*>(curves(view)[1])->dataSize(), 2U);
        stimuli->item(1, 0)->setText("6");
        finish(view);
        auto* q = static_cast<QwtPlotCurve*>(curves(view)[1]);
        QCOMPARE(q->dataSize(), 6U);
        for (int i = 1; i < timing_gui_limits::inputs; ++i)
            click(view, "timing_inputs_add");
        click(view, "timing_inputs_add");
        QCOMPARE(table(view, "timing_inputs")->rowCount(), timing_gui_limits::inputs);
        QVERIFY(status(view).contains("limit"));
    }
};
QTEST_MAIN(TimingGuiTest)
#include "timing_gui_test.moc"
