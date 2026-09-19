#include "communications_view.hpp"
#include "main_window.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QtTest>
#include <cmath>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_marker.h>
#include <qwt_symbol.h>
#include <qwt_text.h>
using namespace openece::gui;
class CommunicationsGuiTest : public QObject {
    Q_OBJECT
    template <class T> T* widget(QWidget& view, const char* name) {
        return view.findChild<T*>(name);
    }
    void text(QWidget& v, const char* name, const QString& value) {
        widget<QLineEdit>(v, name)->setText(value);
    }
    void click(QWidget& v, const char* name) { widget<QPushButton>(v, name)->click(); }
    void spin(QWidget& v, const char* name, int value) {
        widget<QSpinBox>(v, name)->setValue(value);
    }
    QString status(QWidget& v) { return widget<QLabel>(v, "comm_status")->text(); }
    QTableWidget* results(QWidget& v) { return widget<QTableWidget>(v, "comm_ber_results"); }
    void ber(QWidget& v, int count = 3, int budget = 12000) {
        spin(v, "comm_points", count);
        spin(v, "comm_budget", budget);
        text(v, "comm_start", count == 1 ? "20" : "-2");
        text(v, "comm_stop", count == 1 ? "20" : "4");
    }
    QStringList counts(QWidget& v) {
        QStringList values;
        for (int row = 0; row < results(v)->rowCount(); ++row)
            for (int col : {1, 2})
                values << results(v)->item(row, col)->data(Qt::UserRole).toString();
        return values;
    }
  private Q_SLOTS:
    void initialLinkAndNoiselessGrayMapping() {
        CommunicationsView v;
        QVERIFY(status(v).startsWith("Link complete"));
        widget<QComboBox>(v, "comm_source")->setCurrentIndex(1);
        widget<QCheckBox>(v, "comm_noise")->setChecked(false);
        text(v, "comm_manual", "00 01 11 10");
        click(v, "comm_simulate");
        QVERIFY(status(v).contains("0 errors in 8 bits; 4 symbols"));
        auto* bits = widget<QTableWidget>(v, "comm_bits");
        QCOMPARE(bits->rowCount(), 8);
        for (int row = 0; row < 8; ++row)
            QCOMPARE(bits->item(row, 1)->text(), bits->item(row, 2)->text());
        auto* plot = widget<QwtPlot>(v, "comm_constellation");
        QCOMPARE(plot->itemList(QwtPlotItem::Rtti_PlotMarker).size(), 4);
        for (auto* item : plot->itemList(QwtPlotItem::Rtti_PlotCurve))
            QCOMPARE(static_cast<QwtPlotCurve*>(item)->dataSize(), std::size_t{4});
    }
    void InvalidBitsOddLengthsAndRecovery() {
        CommunicationsView v;
        widget<QComboBox>(v, "comm_source")->setCurrentIndex(1);
        text(v, "comm_manual", "010");
        click(v, "comm_simulate");
        QVERIFY(status(v).contains("even"));
        QCOMPARE(widget<QLineEdit>(v, "comm_manual")->text(), QString("010"));
        QCOMPARE(widget<QTableWidget>(v, "comm_bits")->rowCount(), 0);
        text(v, "comm_manual", QString::fromUtf8("01π0"));
        click(v, "comm_simulate");
        QVERIFY(status(v).contains("only 0, 1"));
        text(v, "comm_manual", "0011");
        click(v, "comm_simulate");
        QVERIFY(status(v).startsWith("Link complete"));
        widget<QComboBox>(v, "comm_source")->setCurrentIndex(0);
        spin(v, "comm_bit_count", 3);
        click(v, "comm_simulate");
        QVERIFY(status(v).contains("even"));
        ber(v, 1, 3);
        click(v, "comm_step");
        QVERIFY(status(v).contains("even"));
        QCOMPARE(results(v)->rowCount(), 0);
    }
    void UnitsPreserveValuesAndPendingInvalidText() {
        CommunicationsView v;
        auto* unit = widget<QComboBox>(v, "comm_rate_unit");
        unit->setCurrentIndex(0);
        QCOMPARE(widget<QLineEdit>(v, "comm_rate")->text(), QString("1000"));
        click(v, "comm_simulate");
        QVERIFY(status(v).contains("Rs=1000"));
        text(v, "comm_rate", "1e-");
        unit->setCurrentIndex(2);
        QCOMPARE(unit->currentIndex(), 0);
        QCOMPARE(widget<QLineEdit>(v, "comm_rate")->text(), QString("1e-"));
        QVERIFY(status(v).contains("Invalid pending"));
        click(v, "comm_simulate");
        QVERIFY(status(v).contains("Invalid numeric"));
        text(v, "comm_rate", "1000");
        unit->setCurrentIndex(2);
        QCOMPARE(widget<QLineEdit>(v, "comm_rate")->text().toDouble(), .001);
        text(v, "comm_rate", "1e308");
        unit->setCurrentIndex(0);
        QCOMPARE(unit->currentIndex(), 2);
        QCOMPARE(widget<QLineEdit>(v, "comm_rate")->text(), QString("1e308"));
    }
    void ReproducibilityAndRunStepEquality() {
        CommunicationsView run, step;
        ber(run);
        ber(step);
        click(run, "comm_run");
        for (int i = 0; i < 12 && !status(step).startsWith("BER complete"); ++i)
            click(step, "comm_step");
        QTRY_VERIFY_WITH_TIMEOUT(status(run).startsWith("BER complete"), 10000);
        QVERIFY(status(step).startsWith("BER complete"));
        QCOMPARE(counts(run), counts(step));
        auto first = counts(run);
        click(run, "comm_run");
        QTRY_VERIFY_WITH_TIMEOUT(status(run).startsWith("BER complete"), 10000);
        QCOMPARE(counts(run), first);
    }
    void CancelRetainsPartialRowsResumeAndEditInvalidation() {
        CommunicationsView v;
        ber(v, 3, 20000);
        click(v, "comm_step");
        QCOMPARE(results(v)->rowCount(), 3);
        click(v, "comm_run");
        click(v, "comm_cancel");
        auto partial = counts(v);
        QVERIFY(status(v).startsWith("BER cancelled"));
        QCOMPARE(results(v)->item(0, 5)->text(), QString("Cancelled (partial)"));
        QCOMPARE(results(v)->item(1, 5)->text(), QString("Not evaluated (cancelled)"));
        QTest::qWait(20);
        QCOMPARE(counts(v), partial);
        click(v, "comm_run");
        QTRY_VERIFY_WITH_TIMEOUT(status(v).startsWith("BER complete"), 10000);
        CommunicationsView reference;
        ber(reference, 3, 20000);
        click(reference, "comm_run");
        QTRY_VERIFY_WITH_TIMEOUT(status(reference).startsWith("BER complete"), 10000);
        QCOMPARE(counts(v), counts(reference));
        text(v, "comm_noise_seed", "9");
        QCOMPARE(results(v)->rowCount(), 0);
        QVERIFY(widget<QwtPlot>(v, "comm_ber_plot")->itemList(QwtPlotItem::Rtti_PlotCurve).empty());
        click(v, "comm_run");
        text(v, "comm_noise_seed", "10");
        QTest::qWait(20);
        QCOMPARE(results(v)->rowCount(), 0);
        auto active = std::make_unique<CommunicationsView>();
        ber(*active, 3, 20000);
        click(*active, "comm_run");
        active.reset();
        QCoreApplication::processEvents();
    }
    void ZeroErrorsUseUpperBoundMarkerWithoutInventedFloor() {
        CommunicationsView v;
        ber(v, 1, 10000);
        click(v, "comm_run");
        QTRY_VERIFY_WITH_TIMEOUT(status(v).startsWith("BER complete"), 10000);
        QCOMPARE(results(v)->item(0, 1)->text(), QString("0"));
        QVERIFY(results(v)->item(0, 3)->text().contains("0 errors in 10000 bits"));
        bool found = false;
        for (auto* item :
             widget<QwtPlot>(v, "comm_ber_plot")->itemList(QwtPlotItem::Rtti_PlotCurve)) {
            auto* curve = static_cast<QwtPlotCurve*>(item);
            if (curve->title().text().startsWith("0 errors")) {
                found = true;
                QCOMPARE(curve->dataSize(), std::size_t{1});
                QCOMPARE(curve->symbol()->style(), QwtSymbol::DTriangle);
                QVERIFY(std::abs(curve->sample(0).y() -
                                 openece::communications::zero_error_upper_bound95(10000)) < 1e-15);
            }
            if (curve->title().text() == "Measured (complete)")
                QCOMPARE(curve->dataSize(), std::size_t{0});
        }
        QVERIFY(found);
    }
    void ResourceBoundsAndPlotTruncationAreExplicit() {
        CommunicationsView v;
        spin(v, "comm_bit_count", 65536);
        click(v, "comm_simulate");
        QVERIFY(status(v).contains("waveform limit"));
        spin(v, "comm_samples", 1);
        click(v, "comm_simulate");
        QVERIFY(status(v).contains("65536 bits"));
        QVERIFY(status(v).contains("first 2048 samples"));
        QCOMPARE(widget<QTableWidget>(v, "comm_bits")->rowCount(), 256);
        for (auto* item : widget<QwtPlot>(v, "comm_i_plot")->itemList(QwtPlotItem::Rtti_PlotCurve))
            QCOMPARE(static_cast<QwtPlotCurve*>(item)->dataSize(), std::size_t{2048});
        ber(v, 41, 1000000);
        click(v, "comm_run");
        QVERIFY(status(v).contains("aggregate"));
        QCOMPARE(results(v)->rowCount(), 0);
        text(v, "comm_bit_seed", "184467440737095516150");
        QCOMPARE(widget<QLineEdit>(v, "comm_bit_seed")->text(), QString("184467440737095516150"));
        click(v, "comm_simulate");
        QVERIFY(status(v).contains("unsigned 64-bit"));
    }
    void LocaleSeedsAndInvalidGridRecovery() {
        const auto old = QLocale();
        QLocale::setDefault(QLocale(QLocale::German));
        CommunicationsView v;
        QLocale::setDefault(old);
        text(v, "comm_rate", "1,5");
        click(v, "comm_simulate");
        QVERIFY(status(v).contains("Invalid numeric"));
        text(v, "comm_rate", "1.5");
        click(v, "comm_simulate");
        QVERIFY(status(v).contains("Rs=1500"));
        ber(v, 1, 1000);
        text(v, "comm_stop", "19");
        click(v, "comm_step");
        QVERIFY(status(v).contains("equal endpoints"));
        text(v, "comm_stop", "20");
        click(v, "comm_step");
        QVERIFY(status(v).startsWith("BER complete"));
        text(v, "comm_start", "nan");
        click(v, "comm_run");
        QVERIFY(status(v).contains("Invalid numeric"));
    }
    void DomainsPreserveDraftsAndPlots() {
        MainWindow window;
        window.show();
        auto* navigation = widget<QListWidget>(window, "domain_navigation");
        QCOMPARE(navigation->count(), 4);
        navigation->setCurrentRow(3);
        text(window, "comm_manual", "pending 01π");
        text(window, "comm_rate", "2");
        click(window, "comm_simulate");
        QVERIFY(status(window).startsWith("Link complete"));
        auto curves =
            widget<QwtPlot>(window, "comm_constellation")->itemList(QwtPlotItem::Rtti_PlotCurve);
        for (int domain : {0, 1, 2, 3})
            navigation->setCurrentRow(domain);
        QCOMPARE(widget<QLineEdit>(window, "comm_manual")->text(),
                 QString::fromUtf8("pending 01π"));
        QCOMPARE(widget<QLineEdit>(window, "comm_rate")->text(), QString("2"));
        QCOMPARE(
            widget<QwtPlot>(window, "comm_constellation")->itemList(QwtPlotItem::Rtti_PlotCurve),
            curves);
        const auto path = qEnvironmentVariable("OPENECE_COMMUNICATIONS_SCREENSHOT");
        if (!path.isEmpty()) {
            QTest::qWait(30);
            QVERIFY(window.grab().save(path));
            widget<QTabWidget>(window, "comm_tabs")->setCurrentIndex(1);
            QTest::qWait(30);
            QVERIFY(window.grab().save(path + ".constellation.png"));
            ber(window, 7, 100000);
            click(window, "comm_run");
            QTRY_VERIFY_WITH_TIMEOUT(status(window).startsWith("BER complete"), 10000);
            QTest::qWait(30);
            QVERIFY(window.grab().save(path + ".ber.png"));
        }
    }
};
QTEST_MAIN(CommunicationsGuiTest)
#include "communications_gui_test.moc"
