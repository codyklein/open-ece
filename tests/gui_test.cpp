#include "main_window.hpp"
#include <qwt_plot.h>

#include <qwt_plot_curve.h>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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
        QCOMPARE(tabs->count(), 2);
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
