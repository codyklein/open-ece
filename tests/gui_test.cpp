#include "main_window.hpp"
#include <openece/dsp/fft.hpp>
#include <openece/dsp/fir.hpp>
#include <qwt_plot.h>

#include <qwt_plot_curve.h>
#include <qwt_text.h>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QtTest>

#include <cmath>
#include <numbers>
#include <vector>

class WorkbenchTest : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void generatesAndUpdatesPlots() {
        openece::gui::MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* status = window.findChild<QLabel*>("status");
        QVERIFY(status);
        QVERIFY(status->text().startsWith("Ready"));
        QCOMPARE(window.findChild<QComboBox*>("phase_unit")->currentText(), QString("Degrees"));
        QCOMPARE(window.findChild<QComboBox*>("spectral_window")->currentText(),
                 QString("Rectangular"));
        auto* time = window.findChild<QwtPlot*>("time_plot");
        auto* spectrum = window.findChild<QwtPlot*>("spectrum_plot");
        QVERIFY(time);
        QVERIFY(spectrum);
        auto* time_curve =
            static_cast<QwtPlotCurve*>(time->itemList(QwtPlotItem::Rtti_PlotCurve).at(0));
        auto* spectrum_curve =
            static_cast<QwtPlotCurve*>(spectrum->itemList(QwtPlotItem::Rtti_PlotCurve).at(0));
        QCOMPARE(time_curve->dataSize(), 1024U);
        QCOMPARE(spectrum_curve->dataSize(), 513U);
        QVERIFY(std::abs(spectrum_curve->sample(8).y() - 1.0) < 1e-12);

        const auto screenshot = qEnvironmentVariable("OPENECE_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            QVERIFY(window.grab().save(screenshot));
        }
        window.findChild<QDoubleSpinBox*>("amplitude")->setValue(2.0);
        window.findChild<QDoubleSpinBox*>("frequency")->setValue(16.0);
        window.findChild<QLineEdit*>("phase")->setText("90");
        window.findChild<QDoubleSpinBox*>("sample_rate")->setValue(512.0);
        window.findChild<QDoubleSpinBox*>("duration")->setValue(0.5);
        QVERIFY(status->text().startsWith("Parameters changed"));
        QTest::mouseClick(window.findChild<QPushButton*>("generate"), Qt::LeftButton);
        QVERIFY(status->text().startsWith("Ready"));
        QCOMPARE(time_curve->dataSize(), 256U);
        QCOMPARE(spectrum_curve->dataSize(), 129U);
        QVERIFY(std::abs(time_curve->sample(0).y() - 2.0) < 1e-12);
        QCOMPARE(spectrum_curve->sample(8).x(), 16.0);
        QVERIFY(std::abs(spectrum_curve->sample(8).y() - 2.0) < 1e-12);
        auto* tabs = window.findChild<QTabWidget*>();
        QCOMPARE(tabs->count(), 3);
        tabs->setCurrentIndex(1);
        QCOMPARE(tabs->currentIndex(), 1);
    }

    void switchesWindowsWithoutChangingTimeSamples() {
        openece::gui::MainWindow window;
        auto* selection = window.findChild<QComboBox*>("spectral_window");
        auto* status = window.findChild<QLabel*>("status");
        auto* summary = window.findChild<QLabel*>("summary");
        auto* button = window.findChild<QPushButton*>("generate");
        auto* time = window.findChild<QwtPlot*>("time_plot");
        auto* spectrum = window.findChild<QwtPlot*>("spectrum_plot");
        auto* time_curve =
            static_cast<QwtPlotCurve*>(time->itemList(QwtPlotItem::Rtti_PlotCurve).at(0));
        auto* spectrum_curve =
            static_cast<QwtPlotCurve*>(spectrum->itemList(QwtPlotItem::Rtti_PlotCurve).at(0));
        std::vector<QPointF> original_time, original_spectrum;
        for (std::size_t i = 0; i < time_curve->dataSize(); ++i) {
            original_time.push_back(time_curve->sample(static_cast<int>(i)));
        }
        for (std::size_t i = 0; i < spectrum_curve->dataSize(); ++i) {
            original_spectrum.push_back(spectrum_curve->sample(static_cast<int>(i)));
        }
        selection->setCurrentIndex(1);
        QVERIFY(status->text().startsWith("Parameters changed"));
        button->click();
        QVERIFY(status->text().startsWith("Ready"));
        QVERIFY(summary->text().contains("Hann (periodic)"));
        QVERIFY(summary->text().contains("Coherent gain: 0.5"));
        QCOMPARE(time_curve->dataSize(), original_time.size());
        for (std::size_t i = 0; i < original_time.size(); ++i) {
            QCOMPARE(time_curve->sample(static_cast<int>(i)), original_time[i]);
        }
        QVERIFY(std::abs(spectrum_curve->sample(8).y() - 1.0) < 1e-12);
        QVERIFY(std::abs(spectrum_curve->sample(7).y() - 0.5) < 1e-12);
        selection->setCurrentIndex(0);
        button->click();
        QVERIFY(summary->text().contains("Rectangular"));
        for (std::size_t i = 0; i < original_spectrum.size(); ++i) {
            QCOMPARE(spectrum_curve->sample(static_cast<int>(i)), original_spectrum[i]);
        }
    }

    void convertsPhaseUnitsAndAcceptsRadians() {
        openece::gui::MainWindow window;
        auto* phase = window.findChild<QLineEdit*>("phase");
        auto* units = window.findChild<QComboBox*>("phase_unit");
        auto* button = window.findChild<QPushButton*>("generate");
        auto* plot = window.findChild<QwtPlot*>("time_plot");
        auto* curve = static_cast<QwtPlotCurve*>(plot->itemList(QwtPlotItem::Rtti_PlotCurve).at(0));
        for (double degrees : {60.0, -135.0, 360.0, -360.0}) {
            phase->setText(QString::number(degrees, 'g', 17));
            button->click();
            const double before = curve->sample(0).y();
            units->setCurrentIndex(1);
            QVERIFY(std::abs(phase->text().toDouble() - degrees * std::numbers::pi / 180.0) <
                    1e-11);
            button->click();
            QVERIFY(std::abs(curve->sample(0).y() - before) < 1e-11);
            units->setCurrentIndex(0);
            QVERIFY(std::abs(phase->text().toDouble() - degrees) < 1e-7);
        }
        units->setCurrentIndex(1);
        phase->setText("-pi/6");
        window.findChild<QComboBox*>("spectral_window")->setCurrentIndex(1);
        button->click();
        QVERIFY(std::abs(curve->sample(0).y() + 0.5) < 1e-11);
        units->setCurrentIndex(0);
        QVERIFY(std::abs(phase->text().toDouble() + 30.0) < 1e-7);
        button->click();
        QVERIFY(std::abs(curve->sample(0).y() + 0.5) < 1e-9);
    }

    void unitSwitchCommitsPendingPhaseTextInOldUnits() {
        openece::gui::MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* phase = window.findChild<QLineEdit*>("phase");
        auto* units = window.findChild<QComboBox*>("phase_unit");
        phase->setFocus();
        phase->selectAll();
        QTest::keyClicks(phase, "45");
        // The unit switch must read the current text without waiting for focus loss.
        units->setCurrentIndex(1);
        QVERIFY(std::abs(phase->text().toDouble() - std::numbers::pi / 4.0) < 1e-11);
        phase->selectAll();
        QTest::keyClicks(phase, "-1.5");
        units->setCurrentIndex(0);
        QVERIFY(std::abs(phase->text().toDouble() - (-1.5 * 180.0 / std::numbers::pi)) < 1e-8);
    }

    void piInputGeneratesAndSwitchesUnits() {
        openece::gui::MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* phase = window.findChild<QLineEdit*>("phase");
        auto* units = window.findChild<QComboBox*>("phase_unit");
        auto* button = window.findChild<QPushButton*>("generate");
        auto* plot = window.findChild<QwtPlot*>("time_plot");
        auto* curve = static_cast<QwtPlotCurve*>(plot->itemList(QwtPlotItem::Rtti_PlotCurve).at(0));
        units->setCurrentIndex(1);
        phase->setFocus();
        phase->selectAll();
        QTest::keyClicks(phase, "pi/2");
        // Direct invocation does not rely on a focus change to commit input.
        button->click();
        QVERIFY(window.findChild<QLabel*>("status")->text().startsWith("Ready"));
        QVERIFY(std::abs(curve->sample(0).y() - 1.0) < 1e-12);
        QCOMPARE(phase->text(), QString("pi/2"));
        units->setCurrentIndex(0);
        QVERIFY(std::abs(phase->text().toDouble() - 90.0) < 1e-12);
        units->setCurrentIndex(1);
        phase->setText("3π/4");
        units->setCurrentIndex(0);
        QVERIFY(std::abs(phase->text().toDouble() - 135.0) < 1e-12);
        units->setCurrentIndex(1);
        phase->setText("-pi/2");
        QTest::keyClick(phase, Qt::Key_Return);
        QVERIFY(std::abs(curve->sample(0).y() + 1.0) < 1e-12);
    }

    void invalidPhaseIsRetainedAndUnitSwitchRollsBack() {
        openece::gui::MainWindow window;
        auto* phase = window.findChild<QLineEdit*>("phase");
        auto* units = window.findChild<QComboBox*>("phase_unit");
        auto* button = window.findChild<QPushButton*>("generate");
        auto* status = window.findChild<QLabel*>("status");
        auto* plot = window.findChild<QwtPlot*>("time_plot");
        auto* curve = static_cast<QwtPlotCurve*>(plot->itemList(QwtPlotItem::Rtti_PlotCurve).at(0));
        units->setCurrentIndex(1);
        for (const QString& text :
             {QString("pi/"), QString("pi/0"), QString("pi+1"), QString("3*pi")}) {
            phase->setText(text);
            button->click();
            QVERIFY(status->text().startsWith("Cannot generate"));
            QCOMPARE(curve->dataSize(), 0U);
            QCOMPARE(phase->text(), text);
            units->setCurrentIndex(0);
            QCOMPARE(units->currentIndex(), 1);
            QVERIFY(status->text().startsWith("Cannot change phase units"));
            QCOMPARE(phase->text(), text);
        }
        phase->setText("pi");
        units->setCurrentIndex(0);
        QCOMPARE(units->currentIndex(), 0);
        QVERIFY(std::abs(phase->text().toDouble() - 180.0) < 1e-12);
        phase->setText("pi/2");
        units->setCurrentIndex(1);
        QCOMPARE(units->currentIndex(), 0);
        QVERIFY(status->text().contains("Degrees accepts decimals"));
        phase->setText("90");
        button->click();
        QVERIFY(status->text().startsWith("Ready"));
        QVERIFY(std::abs(curve->sample(0).y() - 1.0) < 1e-12);
    }

    void piButtonInsertsAtCursorAndReplacesSelection() {
        openece::gui::MainWindow window;
        auto* phase = window.findChild<QLineEdit*>("phase");
        auto* units = window.findChild<QComboBox*>("phase_unit");
        auto* insert = window.findChild<QPushButton*>("insert_pi");
        QVERIFY(!insert->isEnabled());
        units->setCurrentIndex(1);
        QVERIFY(insert->isEnabled());
        phase->selectAll();
        insert->click();
        QCOMPARE(phase->text(), QString("π"));
        phase->setText("3/4");
        phase->setCursorPosition(1);
        insert->click();
        QCOMPARE(phase->text(), QString("3π/4"));
        units->setCurrentIndex(0);
        QVERIFY(!insert->isEnabled());
        QVERIFY(std::abs(phase->text().toDouble() - 135.0) < 1e-12);
    }

    void firImpulseShowsFullRecordDelayAndResponse() {
        openece::gui::MainWindow window;
        auto* mode = window.findChild<QComboBox*>("filter_type");
        auto* taps = window.findChild<QSpinBox*>("filter_taps");
        auto* cutoff = window.findChild<QDoubleSpinBox*>("filter_cutoff");
        auto* button = window.findChild<QPushButton*>("generate");
        QVERIFY(!taps->isEnabled());
        QVERIFY(!cutoff->isEnabled());
        mode->setCurrentIndex(1);
        QVERIFY(taps->isEnabled());
        QVERIFY(cutoff->isEnabled());
        taps->setValue(3);
        cutoff->setValue(25.);
        window.findChild<QDoubleSpinBox*>("sample_rate")->setValue(100.);
        window.findChild<QDoubleSpinBox*>("frequency")->setValue(0.);
        window.findChild<QDoubleSpinBox*>("duration")->setValue(.01);
        window.findChild<QLineEdit*>("phase")->setText("90");
        button->click();
        QVERIFY(window.findChild<QLabel*>("status")->text().startsWith("Ready"));
        const auto curves =
            window.findChild<QwtPlot*>("time_plot")->itemList(QwtPlotItem::Rtti_PlotCurve);
        QCOMPARE(curves.size(), 2);
        auto* original = static_cast<QwtPlotCurve*>(curves[0]);
        auto* filtered = static_cast<QwtPlotCurve*>(curves[1]);
        QVERIFY(original->testLegendAttribute(QwtPlotCurve::LegendShowLine));
        QVERIFY(filtered->testLegendAttribute(QwtPlotCurve::LegendShowLine));
        QVERIFY(original->legendIconSize().width() > 0);
        QVERIFY(filtered->legendIconSize().width() > 0);
        QCOMPARE(original->title().color(), original->pen().color());
        QCOMPARE(filtered->title().color(), filtered->pen().color());
        QCOMPARE(original->dataSize(), 1U);
        QCOMPARE(filtered->dataSize(), 3U);
        const double edge = .08 / std::numbers::pi;
        const std::vector expected{edge / (.5 + 2 * edge), .5 / (.5 + 2 * edge),
                                   edge / (.5 + 2 * edge)};
        for (int k = 0; k < 3; ++k) {
            QCOMPARE(filtered->sample(k).x(), static_cast<double>(k) / 100.);
            QVERIFY(std::abs(filtered->sample(k).y() - expected[static_cast<std::size_t>(k)]) <
                    1e-14);
        }
        QVERIFY(filtered->sample(1).y() > filtered->sample(0).y());
        QVERIFY(window.findChild<QLabel*>("summary")->text().contains("Group delay: 1 samples"));
        QVERIFY(
            window.findChild<QLabel*>("summary")->text().contains("Duration extension: 0.02 s"));
        QVERIFY(
            window.findChild<QLabel*>("summary")->text().contains("no fully immersed interval"));
        const auto spectrum_curves =
            window.findChild<QwtPlot*>("spectrum_plot")->itemList(QwtPlotItem::Rtti_PlotCurve);
        auto* spectrum = static_cast<QwtPlotCurve*>(spectrum_curves[1]);
        QCOMPARE(spectrum->dataSize(), 3U); // 3 samples padded to 4, preserving normalization by 3.
        QVERIFY(std::abs(spectrum->sample(0).y() - 1. / 3.) < 1e-14);
        QCOMPARE(spectrum->sample(2).x(), 50.);
        auto* response =
            static_cast<QwtPlotCurve*>(window.findChild<QwtPlot*>("filter_response_plot")
                                           ->itemList(QwtPlotItem::Rtti_PlotCurve)[0]);
        QCOMPARE(response->dataSize(), 1025U);
        QCOMPARE(response->sample(0).x(), 0.);
        QCOMPARE(response->sample(1024).x(), 50.);
        for (int k = 0; k <= 1024; ++k) {
            const double omega = std::numbers::pi * static_cast<double>(k) / 1024.;
            const double magnitude = std::abs(expected[1] + 2 * expected[0] * std::cos(omega));
            const double db = 20 * std::log10(std::max(1e-6, magnitude));
            QVERIFY(std::abs(response->sample(k).y() - db) < 1e-12);
        }
        QVERIFY(window.findChild<QLabel*>("response_summary")->text().contains("-120 dB"));
    }

    void firControlsWindowsBypassAndErrorRecovery() {
        openece::gui::MainWindow window;
        auto* mode = window.findChild<QComboBox*>("filter_type");
        auto* taps = window.findChild<QSpinBox*>("filter_taps");
        auto* cutoff = window.findChild<QDoubleSpinBox*>("filter_cutoff");
        auto* button = window.findChild<QPushButton*>("generate");
        auto* status = window.findChild<QLabel*>("status");
        mode->setCurrentIndex(1);
        button->click();
        QVERIFY(status->text().startsWith("Ready"));
        auto* time = window.findChild<QwtPlot*>("time_plot");
        auto* spectrum_plot = window.findChild<QwtPlot*>("spectrum_plot");
        auto* filtered = static_cast<QwtPlotCurve*>(time->itemList(QwtPlotItem::Rtti_PlotCurve)[1]);
        auto* filtered_spectrum =
            static_cast<QwtPlotCurve*>(spectrum_plot->itemList(QwtPlotItem::Rtti_PlotCurve)[1]);
        QCOMPARE(filtered->dataSize(), 1086U);
        auto* initial_response =
            static_cast<QwtPlotCurve*>(window.findChild<QwtPlot*>("filter_response_plot")
                                           ->itemList(QwtPlotItem::Rtti_PlotCurve)[0]);
        bool reached_floor = false;
        for (std::size_t k = 0; k < initial_response->dataSize(); ++k) {
            const double db = initial_response->sample(static_cast<int>(k)).y();
            QVERIFY(std::isfinite(db));
            QVERIFY(db >= -120.);
            reached_floor = reached_floor || db == -120.;
        }
        QVERIFY(reached_floor);
        std::vector<double> values;
        for (std::size_t n = 0; n < filtered->dataSize(); ++n)
            values.push_back(filtered->sample(static_cast<int>(n)).y());
        window.findChild<QComboBox*>("spectral_window")->setCurrentIndex(1);
        QVERIFY(status->text().startsWith("Parameters changed"));
        button->click();
        const auto expected_spectrum = openece::dsp::amplitude_spectrum(
            openece::SampledSignal(values, 1024.), openece::dsp::Window::Hann);
        for (std::size_t n = 0; n < values.size(); ++n)
            QCOMPARE(filtered->sample(static_cast<int>(n)).y(), values[n]);
        QCOMPARE(filtered_spectrum->dataSize(), expected_spectrum.amplitudes.size());
        for (std::size_t k = 0; k < expected_spectrum.amplitudes.size(); ++k) {
            QVERIFY(std::abs(filtered_spectrum->sample(static_cast<int>(k)).y() -
                             expected_spectrum.amplitudes[k]) < 1e-13);
        }
        // A changed rate redesigns in Hz, rather than reusing stale coefficients.
        window.findChild<QDoubleSpinBox*>("sample_rate")->setValue(512.);
        button->click();
        QCOMPARE(filtered->dataSize(), 574U);
        auto* response =
            static_cast<QwtPlotCurve*>(window.findChild<QwtPlot*>("filter_response_plot")
                                           ->itemList(QwtPlotItem::Rtti_PlotCurve)[0]);
        QCOMPARE(response->sample(1024).x(), 256.);
        const auto redesigned = openece::dsp::frequency_response(
            openece::dsp::design_lowpass(63, 64., 512.), 512., 1025);
        QVERIFY(std::abs(response->sample(256).y() -
                         20 * std::log10(std::abs(redesigned.values[256]))) < 1e-12);
        cutoff->setValue(256.);
        button->click();
        QVERIFY(status->text().startsWith("Cannot generate"));
        for (const char* name : {"time_plot", "spectrum_plot", "filter_response_plot"}) {
            for (auto* item :
                 window.findChild<QwtPlot*>(name)->itemList(QwtPlotItem::Rtti_PlotCurve))
                QCOMPARE(static_cast<QwtPlotCurve*>(item)->dataSize(), 0U);
        }
        cutoff->setValue(64.);
        taps->setValue(4); // Typed even values are rejected, not silently changed.
        button->click();
        QVERIFY(status->text().contains("odd tap"));
        taps->setValue(31);
        button->click();
        QVERIFY(status->text().startsWith("Ready"));
        window.findChild<QLineEdit*>("phase")->setText("pi/");
        window.findChild<QComboBox*>("phase_unit")->setCurrentIndex(1);
        QCOMPARE(response->dataSize(), 0U);
        QCOMPARE(filtered->dataSize(), 0U);
        window.findChild<QLineEdit*>("phase")->setText("0");
        mode->setCurrentIndex(0);
        button->click();
        QVERIFY(status->text().startsWith("Ready"));
        QVERIFY(!filtered->isVisible());
        QCOMPARE(filtered->dataSize(), 0U);
        QCOMPARE(filtered_spectrum->dataSize(), 0U);
        QCOMPARE(response->dataSize(), 0U);
        QVERIFY(!taps->isEnabled());
        auto* original = static_cast<QwtPlotCurve*>(time->itemList(QwtPlotItem::Rtti_PlotCurve)[0]);
        QCOMPARE(original->dataSize(), 512U);
    }

    void firOutputLimitAndScreenshots() {
        openece::gui::MainWindow window;
        auto* mode = window.findChild<QComboBox*>("filter_type");
        auto* taps = window.findChild<QSpinBox*>("filter_taps");
        auto* button = window.findChild<QPushButton*>("generate");
        auto* duration = window.findChild<QDoubleSpinBox*>("duration");
        auto* status = window.findChild<QLabel*>("status");
        mode->setCurrentIndex(1);
        taps->setValue(3);
        // Choose fs=1000 for exact millisecond duration control.
        window.findChild<QDoubleSpinBox*>("sample_rate")->setValue(1000.);
        duration->setValue(65.534);
        button->click();
        QVERIFY(status->text().startsWith("Ready"));
        auto* filtered = static_cast<QwtPlotCurve*>(
            window.findChild<QwtPlot*>("time_plot")->itemList(QwtPlotItem::Rtti_PlotCurve)[1]);
        QCOMPARE(filtered->dataSize(), 65536U);
        duration->setValue(65.535);
        button->click();
        QVERIFY(status->text().contains("Full FIR output exceeds"));
        QCOMPARE(filtered->dataSize(), 0U);
        duration->setValue(.25);
        taps->setValue(63);
        window.findChild<QDoubleSpinBox*>("frequency")->setValue(100.);
        button->click();
        QVERIFY(status->text().startsWith("Ready"));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto path = qEnvironmentVariable("OPENECE_FIR_SCREENSHOT");
        if (!path.isEmpty()) {
            QVERIFY(window.grab().save(path + "-signals.png"));
            window.findChild<QTabWidget*>()->setCurrentIndex(2);
            QCoreApplication::processEvents();
            QVERIFY(window.grab().save(path + "-response.png"));
        }
    }

    void rejectsInvalidInputAndRecovers() {
        openece::gui::MainWindow window;
        auto* frequency = window.findChild<QDoubleSpinBox*>("frequency");
        auto* duration = window.findChild<QDoubleSpinBox*>("duration");
        auto* button = window.findChild<QPushButton*>("generate");
        auto* status = window.findChild<QLabel*>("status");
        auto* plot = window.findChild<QwtPlot*>("time_plot");
        auto* curve = static_cast<QwtPlotCurve*>(plot->itemList(QwtPlotItem::Rtti_PlotCurve).at(0));
        frequency->setValue(600.0);
        button->click();
        QVERIFY(status->text().contains("Nyquist"));
        QCOMPARE(curve->dataSize(), 0U);
        frequency->setValue(8.0);
        duration->setValue(100.0);
        button->click();
        QVERIFY(status->text().contains("65,536"));
        duration->setValue(0.000001);
        button->click();
        QVERIFY(status->text().startsWith("Cannot generate"));
        duration->setValue(1.0);
        button->click();
        QVERIFY(status->text().startsWith("Ready"));
        QCOMPARE(curve->dataSize(), 1024U);
        duration->setValue(64.0);
        button->click();
        QVERIFY(status->text().startsWith("Ready"));
        QCOMPARE(curve->dataSize(), 65536U);
        frequency->setValue(0.0);
        window.findChild<QDoubleSpinBox*>("sample_rate")->setValue(1.0);
        window.findChild<QLineEdit*>("phase")->setText("90");
        duration->setValue(1.0);
        button->click();
        QVERIFY(status->text().startsWith("Ready"));
        QCOMPARE(curve->dataSize(), 1U);
        QVERIFY(curve->symbol() != nullptr);
        QVERIFY(std::abs(curve->sample(0).y() - 1.0) < 1e-12);
    }
};

QTEST_MAIN(WorkbenchTest)
#include "gui_test.moc"
