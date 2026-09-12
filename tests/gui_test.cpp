#include "main_window.hpp"
#include <qwt_plot.h>

#include <qwt_plot_curve.h>

#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QtTest>

#include <cmath>

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
        window.findChild<QDoubleSpinBox*>("phase")->setValue(90.0);
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
        window.findChild<QDoubleSpinBox*>("phase")->setValue(90.0);
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
