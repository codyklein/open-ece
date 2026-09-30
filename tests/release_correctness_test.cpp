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
#include <QWheelEvent>
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
QList<QPolygonF> plotSamples(QObject& v) {
    QList<QPolygonF> result;
    for (const auto* name : {"time_plot", "spectrum_plot", "filter_response_plot"})
        for (auto* item : control<QwtPlot>(v, name)->itemList(QwtPlotItem::Rtti_PlotCurve)) {
            auto* curve = static_cast<QwtPlotCurve*>(item);
            QPolygonF samples;
            for (std::size_t i = 0; i < curve->dataSize(); ++i)
                samples.append(curve->sample(static_cast<int>(i)));
            result.append(samples);
        }
    return result;
}
void stepUp(QWidget* spin, bool wheel) {
    if (wheel) {
        const QPointF local = spin->rect().center();
        QWheelEvent event(local, spin->mapToGlobal(local.toPoint()), {}, {0, 120}, Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(spin, &event);
    } else
        QTest::keyClick(spin, Qt::Key_Up);
}
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
    void acceptedSignalStepMarksStale_data() {
        QTest::addColumn<bool>("wheel");
        QTest::newRow("keyboard-up") << false;
        QTest::newRow("mouse-wheel-up") << true;
    }
    void acceptedSignalStepMarksStale() {
        QFETCH(bool, wheel);
        auto p = openece::project::default_project();
        p.signals.frequency.text = "20";
        p.signals.filter = "fir_lowpass";
        p.signals.cutoff.text = "40";
        p.signals.taps_text = "127";
        ProjectDocument document(std::make_unique<ProjectWorkspace>(p));
        auto& w = document.workspace();
        w.show();
        QApplication::processEvents();
        click(w, "generate");
        auto* status = control<QLabel>(w, "status");
        QVERIFY(status->text().contains("plots match the current parameters"));
        QVERIFY(!document.dirty());
        const auto original = plotSamples(w);
        auto* spin = control<QDoubleSpinBox>(w, "frequency");
        spin->setFocus();
        // No erase/intermediate invalid buffer: each accepted step bypasses the
        // inner editor's textChanged signal on Qt 6.11.2.
        for (int i = 0; i < 5; ++i)
            stepUp(spin, wheel);
        QCOMPARE(static_cast<DraftDouble*>(spin)->raw_text(), QString("25.0000"));
        QVERIFY2(status->text().contains("stale"), qPrintable(status->text()));
        QCOMPARE(plotSamples(w), original);
        QVERIFY(document.dirty());
        const auto edited = w.capture();
        QCOMPARE(edited.signals.frequency.text, std::string("25.0000"));
        const auto revision = document.revision();
        click(w, "generate");
        QVERIFY(status->text().contains("plots match the current parameters"));
        QVERIFY(plotSamples(w) != original);
        QCOMPARE(w.capture(), edited);
        QCOMPARE(document.revision(), revision); // Calculation is not a persisted edit.
    }
    void signalEditorPathsMarkStale_data() {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("method");
        const std::pair<const char*, const char*> fields[] = {
            {"amplitude", "2"},  {"frequency", "25"},     {"sample_rate", "2048"},
            {"duration", "0.5"}, {"filter_cutoff", "45"}, {"filter_taps", "129"},
            {"phase", "45"}};
        for (const auto& [name, valid] : fields) {
            QTest::newRow(qPrintable(QString(name) + "-typed"))
                << QString(name) << QString(valid) << QString("typing");
            QTest::newRow(qPrintable(QString(name) + "-pending"))
                << QString(name) << QString(name == QString("phase") ? "3*pi/" : "1e-")
                << QString("typing");
            if (name != QString("phase"))
                for (const auto* method : {"keyboard-step", "wheel-step"})
                    QTest::newRow(qPrintable(QString(name) + "-" + method))
                        << QString(name) << QString{} << QString(method);
        }
    }
    void signalEditorPathsMarkStale() {
        QFETCH(QString, name);
        QFETCH(QString, input);
        QFETCH(QString, method);
        const bool step = method != "typing";
        auto p = openece::project::default_project();
        p.signals.filter = "fir_lowpass";
        SignalsDspView v(nullptr, &p.signals, true);
        v.show();
        QApplication::processEvents();
        click(v, "generate");
        auto* status = control<QLabel>(v, "status");
        QVERIFY(status->text().contains("plots match the current parameters"));
        const auto original = plotSamples(v);
        auto* object = v.findChild<QWidget*>(name);
        QVERIFY(object);
        auto* editor = qobject_cast<QLineEdit*>(object);
        if (!editor)
            editor = object->findChild<QLineEdit*>();
        QVERIFY(editor);
        editor->setFocus();
        if (step) {
            stepUp(object, method == "wheel-step");
        } else {
            QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
            QTest::keyClicks(editor, input); // Replace selection, without first erasing it.
        }
        QVERIFY2(status->text().contains("stale"), qPrintable(status->text()));
        const auto visible = editor->text();
        QTest::keyClick(editor, Qt::Key_Tab);
        QCOMPARE(editor->text(), visible);
        QVERIFY(status->text().contains("stale"));
        QCOMPARE(plotSamples(v), original); // No analysis ran during editing/focus loss.
        v.synchronize_pending_text();
        const auto saved = openece::project::encode_project(p);
        auto restored = openece::project::decode_project(saved).snapshot;
        QCOMPARE(restored, p);
        SignalsDspView inert(nullptr, &restored.signals, true);
        auto* restored_object = inert.findChild<QWidget*>(name);
        auto* restored_editor = qobject_cast<QLineEdit*>(restored_object);
        if (!restored_editor)
            restored_editor = restored_object->findChild<QLineEdit*>();
        QCOMPARE(restored_editor->text(), visible);
        for (const auto& samples : plotSamples(inert))
            QVERIFY(samples.isEmpty());
        QVERIFY(control<QLabel>(inert, "status")->text().isEmpty());
        if (!step) {
            // Raw suffix-free draft text remains exact, including incomplete numbers.
            QString raw = editor->text();
            if (auto* d = qobject_cast<QDoubleSpinBox*>(object))
                raw = static_cast<DraftDouble*>(d)->raw_text();
            else if (auto* i = qobject_cast<QSpinBox*>(object))
                raw = static_cast<DraftInt*>(i)->raw_text();
            QCOMPARE(raw, input);
        }
    }
    void signalSelectorsMarkStale_data() {
        QTest::addColumn<QString>("name");
        for (const char* name : {"phase_unit", "spectral_window", "filter_type"})
            QTest::newRow(name) << QString(name);
    }
    void signalSelectorsMarkStale() {
        QFETCH(QString, name);
        SignalsDspView v;
        v.show();
        QApplication::processEvents();
        const auto original = plotSamples(v);
        auto* box = v.findChild<QComboBox*>(name);
        QVERIFY(box);
        box->setFocus();
        QTest::keyClick(box, Qt::Key_Down);
        QCOMPARE(box->currentIndex(), 1);
        QVERIFY(control<QLabel>(v, "status")->text().contains("stale"));
        QCOMPARE(plotSamples(v), original);
        click(v, "generate");
        QVERIFY(
            control<QLabel>(v, "status")->text().contains("plots match the current parameters"));
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
    void destroyWithActiveDelegate_data() {
        QTest::addColumn<QString>("domain");
        for (const char* name : {"digital", "timing", "dc", "ac"})
            QTest::newRow(name) << QString(name);
    }
    void destroyWithActiveDelegate() {
        QFETCH(QString, domain);
        std::unique_ptr<DraftView> view;
        const char* table_name = nullptr;
        if (domain == "digital") {
            view = std::make_unique<DigitalLogicView>();
            table_name = "digital_inputs";
        } else if (domain == "timing") {
            view = std::make_unique<TimingView>();
            table_name = "timing_inputs";
        } else if (domain == "dc") {
            view = std::make_unique<CircuitsView>();
            table_name = "circuit_nodes";
        } else {
            view = std::make_unique<AcView>();
            table_name = "ac_nodes";
        }
        view->show();
        QApplication::processEvents();
        auto* table = control<QTableWidget>(*view, table_name);
        auto* item = table->item(0, 1);
        QVERIFY(item);
        table->setCurrentItem(item);
        table->editItem(item);
        auto* editor = table->findChild<QLineEdit*>();
        QVERIFY(editor);
        const auto committed = item->text();
        erase(editor);
        QTest::keyClicks(editor, "pending name");
        QCOMPARE(item->text(), committed);
        QSignalSpy edits(view.get(), &DraftView::draftEdited);
        view.reset(); // Focus loss must not call back into destroyed draft members.
        QCOMPARE(edits.count(), 0);
        QApplication::processEvents();
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
