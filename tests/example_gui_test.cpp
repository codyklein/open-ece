#include "main_window.hpp"
#include "project_workspace.hpp"
#include "transient_view.hpp"
#include <QAction>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <cmath>
#include <numbers>
#include <openece/dsp/fir.hpp>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
using namespace openece;
using namespace openece::gui;
namespace {
template <class T> T* get(QObject& root, const char* name) {
    auto* result = root.findChild<T*>(name);
    if (!result)
        qFatal("Missing control %s", name);
    return result;
}
void click(QObject& root, const char* name) { get<QPushButton>(root, name)->click(); }
QString status(QObject& root, const char* name) { return get<QLabel>(root, name)->text(); }
void inert(QObject& root) {
    for (auto name :
         {"digital_truth_table", "timing_results", "circuit_voltages", "circuit_currents",
          "ac_voltages", "ac_currents", "ac_sweep_results", "comm_bits", "comm_ber_results"})
        QCOMPARE(get<QTableWidget>(root, name)->rowCount(), 0);
    for (auto* timer : root.findChildren<QTimer*>())
        QVERIFY(!timer->isActive());
    for (auto* plot : root.findChildren<QwtPlot*>())
        for (auto* item : plot->itemList(QwtPlotItem::Rtti_PlotCurve))
            QCOMPARE(static_cast<QwtPlotCurve*>(item)->dataSize(), std::size_t{0});
}
QByteArray read(const QString& relative) {
    QFile file(QString(OPENECE_SOURCE_DIR) + relative);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Cannot open example");
    return file.readAll();
}
} // namespace
class ExampleGuiTest : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void examples_data() {
        QTest::addColumn<QString>("name");
        for (auto name :
             {"sine-fft-fir", "half-adder", "dff-timing", "dc-divider", "rc-lowpass", "series-rlc",
              "bpsk-link-ber", "qpsk-link-ber", "intentionally-incomplete", "transient-rc-step",
              "transient-rl-response", "transient-rlc-damping", "transient-source-breakpoints"})
            QTest::newRow(name) << QString(name);
    }
    void examples() {
        QFETCH(QString, name);
        const auto p =
            project::decode_project(read("/examples/" + name + ".openece").toStdString()).snapshot;
        auto doc = std::make_unique<ProjectDocument>(std::make_unique<ProjectWorkspace>(p));
        auto* document = doc.get();
        MainWindow window(std::move(doc), {}, std::make_shared<SettingsProjectPreferences>());
        window.resize(1440,
                      qEnvironmentVariableIsEmpty("OPENECE_EXAMPLE_SCREENSHOTS") ? 1200 : 1320);
        window.show();
        QCoreApplication::processEvents();
        auto& w = document->workspace();
        QSignalSpy edits(&w, &DraftView::draftEdited);
        inert(w);
        QVERIFY(w.capture() == p);
        {
            ProjectWorkspace restored(
                project::decode_project(project::encode_project(w.capture())).snapshot);
            QVERIFY(restored.capture() == p);
            inert(restored);
        }
        if (name.startsWith("transient-")) {
            auto* view = get<TransientView>(w, "transient_view");
            QVERIFY(!view->result());
            click(w, "transient_run");
            QTRY_VERIFY_WITH_TIMEOUT(view->result() && view->result()->status ==
                                                           circuits::transient::Status::complete,
                                     20000);
            const auto r = view->result();
            QCOMPARE(r->current_time_seconds, name == "transient-rl-response"   ? .0005
                                              : name == "transient-rlc-damping" ? .0015
                                                                                : .005);
            if (name != "transient-source-breakpoints")
                QVERIFY(r->currents[0].back() < 0);
            if (name == "transient-rc-step")
                QVERIFY(std::abs(r->voltages[0].back() - 10 * (1 - std::exp(-5.))) < .005);
            if (name == "transient-rl-response")
                QVERIFY(std::abs(r->currents[1].back() - .01 * (1 - std::exp(-5.))) < .000005);
            if (name == "transient-rlc-damping") {
                const auto peak = *std::max_element(r->voltages[0].begin(), r->voltages[0].end());
                QVERIFY(std::abs(peak - 1.72925) < .002);
            }
            if (name == "transient-source-breakpoints") {
                for (auto t : {.001, .003}) {
                    auto it = std::find_if(r->times.begin(), r->times.end(),
                                           [&](auto point) { return point.seconds == t; });
                    QVERIFY(it != r->times.end());
                    const auto i = static_cast<std::size_t>(it - r->times.begin());
                    QCOMPARE(r->times[i + 1].seconds, t);
                    QCOMPARE(r->times[i].side, circuits::transient::SampleSide::before_breakpoint);
                    QCOMPARE(r->times[i + 1].side,
                             circuits::transient::SampleSide::after_breakpoint);
                    QCOMPARE(r->voltages[0][i], r->voltages[0][i + 1]);
                    QVERIFY(r->voltages[1][i] != r->voltages[1][i + 1]);
                }
            }
        } else if (name == "sine-fft-fir") {
            click(w, "generate");
            QVERIFY2(!status(w, "status").contains("Failed"), qPrintable(status(w, "status")));
            auto curves = get<QwtPlot>(w, "time_plot")->itemList(QwtPlotItem::Rtti_PlotCurve);
            QCOMPARE(curves.size(), 2);
            QCOMPARE(static_cast<QwtPlotCurve*>(curves[0])->dataSize(), std::size_t{1024});
            QCOMPARE(static_cast<QwtPlotCurve*>(curves[1])->dataSize(), std::size_t{1150});
            auto spectrum = get<QwtPlot>(w, "spectrum_plot")->itemList(QwtPlotItem::Rtti_PlotCurve);
            auto* original = static_cast<QwtPlotCurve*>(spectrum[0]);
            QVERIFY(std::abs(original->sample(20).y() - 1) < 1e-10);
            QCOMPARE(original->sample(20).x(), 20.);
            const auto fs = std::stod(p.signals.sample_rate.text);
            const auto taps = static_cast<std::size_t>(std::stoul(p.signals.taps_text));
            const auto pass = dsp::frequency_response(
                dsp::design_lowpass(taps, std::stod(p.signals.cutoff.text), fs), fs, 513);
            const auto stop = dsp::frequency_response(dsp::design_lowpass(taps, 10, fs), fs, 513);
            QVERIFY(std::abs(std::abs(pass.values[20]) - 1) < .01);
            QVERIFY(std::abs(stop.values[20]) < .05);
            QCOMPARE(static_cast<double>(taps - 1) / (2 * fs), .0615234375);
        } else if (name == "half-adder") {
            click(w, "digital_truth");
            auto* t = get<QTableWidget>(w, "digital_truth_table");
            QCOMPARE(t->rowCount(), 4);
            for (int row = 0; row < 4; ++row) {
                const int a = row / 2, b = row % 2;
                for (auto [col, value] : {std::pair{0, a}, {1, b}, {2, a ^ b}, {3, a & b}})
                    QCOMPARE(t->item(row, col)->text().toInt(), value);
            }
        } else if (name == "dff-timing") {
            click(w, "timing_run");
            QTRY_VERIFY(status(w, "timing_status").startsWith("Complete"));
            auto curves = get<QwtPlot>(w, "timing_diagram")->itemList(QwtPlotItem::Rtti_PlotCurve);
            QCOMPARE(curves.size(), 3);
            auto* q = static_cast<QwtPlotCurve*>(curves[2]);
            QCOMPARE(q->dataSize(), std::size_t{6});
            QCOMPARE(q->sample(2), QPointF(6, .7));
            QCOMPARE(q->sample(4), QPointF(16, 0));
            QCOMPARE(q->sample(5), QPointF(30, 0));
        } else if (name == "dc-divider") {
            click(w, "circuit_solve");
            auto* t = get<QTableWidget>(w, "circuit_voltages");
            QCOMPARE(t->rowCount(), 3);
            QCOMPARE(t->item(1, 1)->text().toDouble(), 10.);
            QCOMPARE(t->item(2, 1)->text().toDouble(), 5.);
            QCOMPARE(get<QTableWidget>(w, "circuit_currents")->item(0, 1)->text().toDouble(),
                     -.005);
        } else if (name == "rc-lowpass" || name == "series-rlc") {
            click(w, "ac_solve");
            auto* t = get<QTableWidget>(w, "ac_voltages");
            const bool rc = name == "rc-lowpass";
            QCOMPARE(t->rowCount(), rc ? 3 : 4);
            const int row = rc ? 2 : 3;
            QVERIFY(std::abs(t->item(row, 3)->text().toDouble() - (rc ? std::sqrt(.5) : 1)) <
                    1e-10);
            QVERIFY(std::abs(t->item(row, 4)->text().toDouble() - (rc ? -45. : 0.)) < 1e-9);
            if (rc) {
                QCOMPARE(t->item(row, 1)->text().toDouble(), .5);
                QCOMPARE(t->item(row, 2)->text().toDouble(), -.5);
                QVERIFY(std::abs(20 * std::log10(t->item(row, 3)->text().toDouble()) +
                                 3.0102999566) < 1e-9);
            } else {
                QVERIFY(
                    std::abs(get<QTableWidget>(w, "ac_currents")->item(0, 1)->text().toDouble() +
                             .01) < 1e-12);
            }
            click(w, "ac_run_sweep");
            // All 201 example points are retained; instrumented/Debug GUI paints are slower.
            QTRY_VERIFY_WITH_TIMEOUT(status(w, "ac_status").startsWith("Sweep complete"), 30000);
            auto* sweep = get<QTableWidget>(w, "ac_sweep_results");
            QCOMPARE(sweep->rowCount(), 201);
            if (!rc) {
                QVERIFY(sweep->item(0, 4)->text().toDouble() > 0);
                QVERIFY(sweep->item(200, 4)->text().toDouble() < 0);
            }
        } else if (name.endsWith("link-ber")) {
            click(w, "comm_simulate");
            QVERIFY(status(w, "comm_status").contains("0 errors in 8 bits"));
            auto* bits = get<QTableWidget>(w, "comm_bits");
            QCOMPARE(bits->rowCount(), 8);
            for (int i = 0; i < 8; ++i)
                QCOMPARE(bits->item(i, 1)->text(), bits->item(i, 2)->text());
            click(w, "comm_run");
            QTRY_VERIFY_WITH_TIMEOUT(status(w, "comm_status").startsWith("BER complete"), 10000);
            auto* ber = get<QTableWidget>(w, "comm_ber_results");
            QCOMPARE(ber->rowCount(), 4);
            for (int row = 0; row < 4; ++row) {
                QCOMPARE(ber->item(row, 2)->data(Qt::UserRole).toULongLong(), 20000ULL);
                QVERIFY(ber->item(row, 1)->data(Qt::UserRole).toULongLong() <= 20000ULL);
                const double theory = .5 * std::erfc(std::sqrt(std::pow(10., row * .4)));
                QVERIFY(std::abs(ber->item(row, 4)->text().toDouble() - theory) < 1e-6);
            }
            QCOMPARE(ber->item(3, 1)->data(Qt::UserRole).toULongLong(), 0ULL);
            QVERIFY(ber->item(3, 3)->text().contains("0 errors"));
        } else {
            for (auto button :
                 {"generate", "digital_evaluate", "timing_step", "circuit_solve", "comm_simulate"})
                click(w, button);
            for (auto label :
                 {"status", "digital_status", "timing_status", "circuit_status", "comm_status"})
                QVERIFY2(status(w, label).contains("Failed"), qPrintable(status(w, label)));
            inert(w);
        }
        QVERIFY(w.capture() == p);
        QCOMPARE(edits.count(), 0);
        QVERIFY(!document->dirty());
        const auto screenshots = qEnvironmentVariable("OPENECE_EXAMPLE_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QCoreApplication::processEvents();
            QVERIFY(QDir().mkpath(screenshots));
            QVERIFY(window.grab().save(screenshots + "/" + name + ".png"));
        }
    }
    void compatibility_data() {
        QTest::addColumn<QString>("name");
        QTest::newRow("complete") << QString("complete");
        QTest::newRow("incomplete") << QString("incomplete");
    }
    void compatibility() {
        QFETCH(QString, name);
        const auto bytes = read("/tests/fixtures/v0.9/" + name + ".openece");
        const auto hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
        QCOMPARE(
            hash,
            name == "complete"
                ? QByteArray("0ba53c8e653ebbbc8720960743b40c3781ec717bbcaa3c718dfe1599a0e0ad0f")
                : QByteArray("0b9b4580268de760fb4b27901fabeb3655ee94af2a4885fc13466e8e7ecc3ac2"));
        auto p = project::decode_project(bytes.toStdString()).snapshot;
        ProjectDocument doc;
        auto candidate = doc.prepare_open(QString(OPENECE_SOURCE_DIR) + "/tests/fixtures/v0.9/" +
                                          name + ".openece");
        QVERIFY(std::holds_alternative<PreparedProject>(candidate));
        QVERIFY(std::holds_alternative<std::monostate>(
            doc.install(std::get<PreparedProject>(std::move(candidate)))));
        auto& w = doc.workspace();
        inert(w);
        QVERIFY(w.capture() == p);
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto saved = temp.filePath("v0.9 reopened π project.openece");
        QVERIFY(std::holds_alternative<SaveStatus>(doc.save_as(saved)));
        QVERIFY(!doc.dirty());
        ProjectDocument again;
        auto reopened = again.prepare_open(saved);
        QVERIFY(std::holds_alternative<PreparedProject>(reopened));
        QVERIFY(std::holds_alternative<std::monostate>(
            again.install(std::get<PreparedProject>(std::move(reopened)))));
        QVERIFY(again.workspace().capture() == p);
        inert(again.workspace());
        if (name == "incomplete") {
            auto ids = project::reserved_ids(p.digital.combinational);
            auto next = project::NextId{77};
            QCOMPARE(project::allocate_id(next, ids).value, 78U);
            QCOMPARE(w.capture().digital.combinational.gates[0].pins[1],
                     project::Reference{project::Id{77}});
        }
    }
    void aboutIsDiscoverable() {
        MainWindow window;
        bool inspected = false;
        QTimer::singleShot(0, &window, [&] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (box) {
                inspected = box->text().contains("MIT") &&
                            box->text().contains("THIRD-PARTY-NOTICES") &&
                            box->text().contains("github.com/codyklein/open-ece") &&
                            box->text().contains(OPENECE_TEST_VERSION);
                box->accept();
            }
        });
        get<QAction>(window, "about_openece")->trigger();
        QVERIFY(inspected);
    }
};
QTEST_MAIN(ExampleGuiTest)
#include "example_gui_test.moc"
