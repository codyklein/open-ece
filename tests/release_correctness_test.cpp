#include "ac_view.hpp"
#include "circuits_view.hpp"
#include "communications_view.hpp"
#include "digital_logic_view.hpp"
#include "project_document.hpp"
#include "project_workflow.hpp"
#include "signals_dsp_view.hpp"
#include "timing_view.hpp"
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QtTest>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_scale_draw.h>
#include <qwt_text.h>
using namespace openece::gui;
namespace {
template <class T> T* control(QObject& v, const char* name) {
    auto* result = v.findChild<T*>(name);
    Q_ASSERT(result);
    return result;
}
void click(QObject& v, const char* name) { control<QPushButton>(v, name)->click(); }
void erase(QLineEdit* editor) {
    editor->setFocus();
    QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier, 0);
    QTest::keyClick(editor, Qt::Key_Backspace, Qt::NoModifier, 0);
}
} // namespace
class ReleaseCorrectnessTest : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void invalidBerBudgetCannotResumePreviousExperiment() {
        CommunicationsView v;
        v.show();
        QApplication::processEvents();
        click(v, "comm_step");
        QVERIFY(control<QTableWidget>(v, "comm_ber_results")->rowCount() > 0);
        auto* budget = control<QSpinBox>(v, "comm_budget");
        QSignalSpy numeric(budget, &QSpinBox::valueChanged);
        erase(budget->findChild<QLineEdit*>());
        QCOMPARE(static_cast<DraftInt*>(budget)->raw_text(), QString{});
        QCOMPARE(numeric.count(), 0);
        click(v, "comm_step");
        QCOMPARE(control<QTableWidget>(v, "comm_ber_results")->rowCount(), 0);
        QVERIFY(control<QLabel>(v, "comm_status")->text().contains("Invalid pending count"));
    }
    void invalidAcPointCountCancelsOldSweep() {
        AcView v;
        v.show();
        QApplication::processEvents();
        click(v, "ac_run_sweep");
        auto* count = control<QSpinBox>(v, "ac_point_count");
        QSignalSpy numeric(count, &QSpinBox::valueChanged);
        erase(count->findChild<QLineEdit*>());
        QVERIFY(static_cast<DraftInt*>(count)->raw_text().isEmpty());
        QCOMPARE(numeric.count(), 0);
        QCOMPARE(control<QTableWidget>(v, "ac_sweep_results")->rowCount(), 0);
        QVERIFY(!v.findChild<QTimer*>()->isActive());
    }
    void pendingSignalNumberMarksRetainedPlotsStale() {
        SignalsDspView v;
        v.show();
        QApplication::processEvents();
        auto* frequency = control<QDoubleSpinBox>(v, "frequency");
        QSignalSpy numeric(frequency, &QDoubleSpinBox::valueChanged);
        erase(frequency->findChild<QLineEdit*>());
        QVERIFY(static_cast<DraftDouble*>(frequency)->raw_text().isEmpty());
        QCOMPARE(numeric.count(), 0);
        QVERIFY(control<QLabel>(v, "status")->text().contains("Parameters changed"));
    }
    void rawCommunicationsFieldsInvalidate_data() {
        QTest::addColumn<QString>("name");
        for (const auto* name : {"comm_bit_count", "comm_samples", "comm_points", "comm_budget",
                                 "comm_manual", "comm_rate", "comm_eb", "comm_bit_seed",
                                 "comm_noise_seed", "comm_start", "comm_stop"})
            QTest::newRow(name) << QString(name);
    }
    void rawCommunicationsFieldsInvalidate() {
        QFETCH(QString, name);
        CommunicationsView v;
        if (name == "comm_manual") {
            control<QComboBox>(v, "comm_source")->setCurrentIndex(1);
            click(v, "comm_simulate");
        }
        v.show();
        QApplication::processEvents();
        click(v, "comm_step");
        click(v, "comm_run");
        QVERIFY(v.findChild<QTimer*>()->isActive());
        auto* object = v.findChild<QWidget*>(name);
        QVERIFY(object);
        auto* editor = qobject_cast<QLineEdit*>(object);
        if (!editor)
            editor = object->findChild<QLineEdit*>();
        QVERIFY(editor);
        QSignalSpy edits(&v, &DraftView::draftEdited);
        erase(editor);
        QVERIFY(edits.count() > 0);
        QCOMPARE(control<QTableWidget>(v, "comm_bits")->rowCount(), 0);
        QCOMPARE(control<QTableWidget>(v, "comm_ber_results")->rowCount(), 0);
        QVERIFY(!v.findChild<QTimer*>()->isActive());
        QApplication::processEvents();
        QCOMPARE(control<QTableWidget>(v, "comm_ber_results")->rowCount(), 0);
    }
    void rawSignalsFieldsMarkStale_data() {
        QTest::addColumn<QString>("name");
        for (const auto* name : {"amplitude", "frequency", "sample_rate", "duration",
                                 "filter_cutoff", "filter_taps", "phase"})
            QTest::newRow(name) << QString(name);
    }
    void rawSignalsFieldsMarkStale() {
        QFETCH(QString, name);
        SignalsDspView v;
        control<QComboBox>(v, "filter_type")->setCurrentIndex(1);
        click(v, "generate");
        v.show();
        QApplication::processEvents();
        auto* object = v.findChild<QWidget*>(name);
        auto* editor = qobject_cast<QLineEdit*>(object);
        if (!editor)
            editor = object->findChild<QLineEdit*>();
        QVERIFY(editor);
        auto* plot = control<QwtPlot>(v, "time_plot");
        auto* curve = static_cast<QwtPlotCurve*>(plot->itemList(QwtPlotItem::Rtti_PlotCurve)[0]);
        const auto samples = curve->dataSize();
        QVERIFY(samples > 0);
        erase(editor);
        QVERIFY(control<QLabel>(v, "status")->text().contains("stale"));
        QCOMPARE(curve->dataSize(), samples); // Old results are retained, explicitly stale.
        const auto pending = editor->text();
        click(v, "generate");
        QCOMPARE(editor->text(), pending);
        QCOMPARE(curve->dataSize(), std::size_t{0});
        QVERIFY(control<QLabel>(v, "status")->text().contains("Cannot generate"));
    }
    void activeCircuitAndTimingDelegatesInvalidate() {
        CircuitsView dc;
        dc.show();
        QApplication::processEvents();
        auto* parts = control<QTableWidget>(dc, "circuit_components");
        auto* editor = qobject_cast<QLineEdit*>(parts->cellWidget(0, 5));
        erase(editor);
        QCOMPARE(control<QTableWidget>(dc, "circuit_voltages")->rowCount(), 0);
        click(dc, "circuit_solve");
        QVERIFY(control<QLabel>(dc, "circuit_status")->text().contains("Invalid numeric"));

        TimingView timing;
        timing.show();
        QApplication::processEvents();
        click(timing, "timing_step");
        QVERIFY(control<QTableWidget>(timing, "timing_results")->rowCount() > 0);
        auto* elements = control<QTableWidget>(timing, "timing_elements");
        control<QTabWidget>(timing, "timing_editor_tabs")->setCurrentIndex(1);
        auto* pin = elements->item(0, 2);
        elements->setCurrentItem(pin);
        elements->editItem(pin);
        auto* pending = elements->findChild<QLineEdit*>();
        QVERIFY(pending);
        const auto committed = pin->text();
        erase(pending);
        QCOMPARE(pin->text(), committed); // Delegate has not committed its buffer.
        QCOMPARE(control<QTableWidget>(timing, "timing_results")->rowCount(), 0);
        click(timing, "timing_step");
        QVERIFY(control<QLabel>(timing, "timing_status")->text().contains("Cannot simulate"));
        QCOMPARE(control<QTableWidget>(timing, "timing_results")->rowCount(), 0);
    }
    void rawDigitalPinCountInvalidates() {
        DigitalLogicView v;
        v.show();
        QApplication::processEvents();
        control<QTableWidget>(v, "digital_gates")->selectRow(0);
        click(v, "digital_truth");
        QVERIFY(control<QTableWidget>(v, "digital_truth_table")->rowCount() > 0);
        auto* count = control<QSpinBox>(v, "digital_pin_count");
        erase(count->findChild<QLineEdit*>());
        QCOMPARE(control<QTableWidget>(v, "digital_truth_table")->rowCount(), 0);
        click(v, "digital_evaluate");
        QVERIFY(control<QLabel>(v, "digital_status")->text().contains("Invalid pending pin count"));
    }
    void resultUpdatesAndNavigationDoNotInvalidateInputsOrDirty() {
        ProjectDocument d;
        auto& w = d.workspace();
        const auto initial = w.capture();
        for (const char* action : {"generate", "digital_evaluate", "timing_step", "circuit_solve",
                                   "ac_solve", "comm_simulate", "comm_step"})
            click(w, action);
        QCOMPARE(w.capture(), initial);
        QVERIFY(!d.dirty());
        auto* ber = control<QTableWidget>(w, "comm_ber_results");
        QVERIFY(ber->rowCount() > 0);
        control<QListWidget>(w, "domain_navigation")->setCurrentRow(3);
        control<QTabWidget>(w, "comm_tabs")->setCurrentIndex(3);
        QVERIFY(d.dirty()); // Persisted navigation edit, not an execution-input edit.
        QVERIFY(ber->rowCount() > 0);
        control<QTabWidget>(w, "comm_tabs")->setCurrentIndex(2);
        auto* budget = control<QSpinBox>(w, "comm_budget");
        erase(budget->findChild<QLineEdit*>());
        QVERIFY(w.capture().communications.budget_text.empty());
        QCOMPARE(ber->rowCount(), 0);
    }
    void projectControlledTextIsPlainAndExact() {
        auto p = openece::project::default_project();
        const QString name = "<b>π 名</b>" + QString(1000, 'x');
        p.circuits.dc.components[0].name = draft_text(name);
        p.circuits.dc.components[0].value.text.clear();
        const QString timing_name = "<b>π 名</b>";
        p.digital.timing.inputs[0].name = draft_text(timing_name);
        ProjectWorkspace w(p);
        for (const char* label : {"status", "summary", "response_summary", "digital_status",
                                  "timing_status", "digital_copy_status", "circuit_status",
                                  "ac_status", "comm_status", "comm_link_summary"})
            QCOMPARE(control<QLabel>(w, label)->textFormat(), Qt::PlainText);
        click(w, "circuit_solve");
        QVERIFY(control<QLabel>(w, "circuit_status")->text().contains(name));
        click(w, "timing_step");
        auto* plot = control<QwtPlot>(w, "timing_diagram");
        const auto curves = plot->itemList(QwtPlotItem::Rtti_PlotCurve);
        QVERIFY(!curves.empty());
        const auto title = static_cast<QwtPlotCurve*>(curves[0])->title();
        QVERIFY(title.text().contains(timing_name));
        QVERIFY(title == QwtText(title.text(), QwtText::PlainText));
        const auto ticks = plot->axisScaleDiv(QwtPlot::yLeft).ticks(QwtScaleDiv::MajorTick);
        bool found = false;
        for (double tick : ticks) {
            const auto label = plot->axisScaleDraw(QwtPlot::yLeft)->label(tick);
            if (label.text().contains(timing_name)) {
                found = true;
                QVERIFY(label == QwtText(label.text(), QwtText::PlainText));
            }
        }
        QVERIFY(found);
        bool plain = false;
        QTimer::singleShot(0, [&] {
            for (auto* widget : QApplication::topLevelWidgets())
                if (auto* box = qobject_cast<QMessageBox*>(widget)) {
                    plain = box->textFormat() == Qt::PlainText && box->text() == name &&
                            box->informativeText().contains(name);
                    box->accept();
                }
        });
        ProjectFailure failure;
        failure.message = name;
        failure.path = name;
        qt_project_dialogs(nullptr)->error(failure);
        QVERIFY(plain);
    }
};
QTEST_MAIN(ReleaseCorrectnessTest)
#include "release_correctness_test.moc"
