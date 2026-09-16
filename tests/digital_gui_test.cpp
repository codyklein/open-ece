#include "digital_logic_view.hpp"
#include "main_window.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QtTest>

using namespace openece::gui;
class DigitalGuiTest : public QObject {
    Q_OBJECT
    template <typename T> T* widget(QWidget& view, const char* name) {
        return view.findChild<T*>(name);
    }
    QTableWidget* gates(QWidget& view) { return widget<QTableWidget>(view, "digital_gates"); }
    QTableWidget* outputs(QWidget& view) { return widget<QTableWidget>(view, "digital_outputs"); }
    void click(QWidget& view, const char* name) {
        widget<QPushButton>(view, name)->click();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    QString status(QWidget& view) { return widget<QLabel>(view, "digital_status")->text(); }
    void source(QWidget& view, const char* name, unsigned id) {
        auto* selector = widget<QComboBox>(view, name);
        const int index = selector->findData(id);
        QVERIFY(index >= 0);
        selector->setCurrentIndex(index);
    }
  private Q_SLOTS:
    void halfAdderEvaluationTruthTableAndDomainPersistence() {
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* navigation = widget<QListWidget>(window, "domain_navigation");
        navigation->setCurrentRow(1);
        QVERIFY(status(window).startsWith("Valid circuit"));
        auto* a = widget<QCheckBox>(window, "digital_input_1");
        auto* b = widget<QCheckBox>(window, "digital_input_2");
        a->setChecked(true);
        b->setChecked(true);
        QCOMPARE(outputs(window)->item(0, 2)->text(), QString("—"));
        click(window, "digital_evaluate");
        QCOMPARE(outputs(window)->item(0, 2)->text(), QString("0"));
        QCOMPARE(outputs(window)->item(1, 2)->text(), QString("1"));
        navigation->setCurrentRow(0);
        navigation->setCurrentRow(1);
        QVERIFY(a->isChecked());
        QVERIFY(b->isChecked());
        QCOMPARE(outputs(window)->item(1, 2)->text(), QString("1"));
        click(window, "digital_truth");
        auto* truth = widget<QTableWidget>(window, "digital_truth_table");
        QCOMPARE(truth->rowCount(), 4);
        QCOMPARE(truth->columnCount(), 4);
        QCOMPARE(truth->horizontalHeaderItem(0)->text(), QString("In: A"));
        QCOMPARE(truth->horizontalHeaderItem(2)->text(), QString("Out: Sum"));
        const QStringList expected{"0000", "0110", "1010", "1101"};
        for (int row = 0; row < 4; ++row) {
            QString actual;
            for (int column = 0; column < 4; ++column)
                actual += truth->item(row, column)->text();
            QCOMPARE(actual, expected[row]);
        }
        QVERIFY(a->isChecked());
        QVERIFY(b->isChecked());
        const auto screenshot = qEnvironmentVariable("OPENECE_DIGITAL_SCREENSHOT");
        if (!screenshot.isEmpty())
            QVERIFY(window.grab().save(screenshot));
        a->setChecked(false);
        QCOMPARE(truth->rowCount(), 0);
        QCOMPARE(outputs(window)->item(0, 2)->text(), QString("—"));
    }
    void missingPinsCyclesAndRecovery() {
        DigitalLogicView view;
        gates(view)->selectRow(0);
        source(view, "digital_pin_0", 0);
        click(view, "digital_evaluate");
        QVERIFY(status(view).contains("Missing source node 0"));
        source(view, "digital_pin_0", 3); // Self-loop, retained as an editable draft.
        click(view, "digital_truth");
        QVERIFY(status(view).contains("unresolved/blocked gate IDs: 3"));
        QCOMPARE(widget<QTableWidget>(view, "digital_truth_table")->rowCount(), 0);
        source(view, "digital_pin_0", 1);
        click(view, "digital_evaluate");
        QVERIFY(status(view).startsWith("Valid circuit"));
        widget<QComboBox>(view, "digital_gate_kind")->setCurrentText("NOT");
        QCOMPARE(widget<QSpinBox>(view, "digital_pin_count")->value(), 2);
        click(view, "digital_evaluate");
        QVERIFY(status(view).contains("Invalid pin count"));
        widget<QSpinBox>(view, "digital_pin_count")->setValue(1);
        click(view, "digital_evaluate");
        QCOMPARE(outputs(view)->item(0, 2)->text(), QString("1"));
    }
    void deletingSourcesNeverRewiresAndAllowsRepair() {
        DigitalLogicView view;
        widget<QTableWidget>(view, "digital_inputs")->selectRow(0);
        click(view, "digital_remove_input");
        QVERIFY(gates(view)->item(0, 2)->text().contains("missing 1"));
        QVERIFY(widget<QComboBox>(view, "digital_pin_0")->currentText().contains("Missing node 1"));
        click(view, "digital_add_input");
        QVERIFY(widget<QComboBox>(view, "digital_pin_0")->currentText().contains("Missing node 1"));
        click(view, "digital_evaluate");
        QVERIFY(status(view).contains("Missing source node 1"));
        source(view, "digital_pin_0", 5);
        gates(view)->selectRow(1);
        source(view, "digital_pin_0", 5);
        click(view, "digital_evaluate");
        QVERIFY(status(view).startsWith("Valid circuit"));
        gates(view)->selectRow(0);
        click(view, "digital_remove_gate");
        QVERIFY(widget<QComboBox>(view, "digital_output_source_0")
                    ->currentText()
                    .contains("Missing node 3"));
        click(view, "digital_evaluate");
        QVERIFY(status(view).contains("Missing source node 3"));
        source(view, "digital_output_source_0", 2);
        click(view, "digital_evaluate");
        QVERIFY(status(view).startsWith("Valid circuit"));
    }
    void addGateMultiInputParityAndPassThroughOutput() {
        DigitalLogicView view;
        click(view, "digital_add_gate");
        QCOMPARE(gates(view)->rowCount(), 3);
        QCOMPARE(gates(view)->currentRow(), 2);
        click(view, "digital_evaluate");
        QVERIFY(status(view).contains("Missing source"));
        widget<QComboBox>(view, "digital_gate_kind")->setCurrentText("XNOR");
        widget<QSpinBox>(view, "digital_pin_count")->setValue(3);
        source(view, "digital_pin_0", 1);
        source(view, "digital_pin_1", 2);
        source(view, "digital_pin_2", 1);
        click(view, "digital_add_output");
        source(view, "digital_output_source_2", 5);
        widget<QCheckBox>(view, "digital_input_1")->setChecked(true);
        click(view, "digital_evaluate");
        QVERIFY(status(view).startsWith("Valid circuit"));
        QCOMPARE(outputs(view)->item(2, 2)->text(), QString("1")); // 1,0,1 even parity.
        source(view, "digital_output_source_2", 1);
        click(view, "digital_evaluate");
        QCOMPARE(outputs(view)->item(2, 2)->text(), QString("1"));
        outputs(view)->selectRow(2);
        click(view, "digital_remove_output");
        QCOMPARE(outputs(view)->rowCount(), 2);
    }
    void exactNamesUnicodeDuplicatesAndRecovery() {
        DigitalLogicView view;
        auto* inputs = widget<QTableWidget>(view, "digital_inputs");
        inputs->item(0, 1)->setText("");
        click(view, "digital_evaluate");
        QVERIFY(status(view).contains("nonempty and unique"));
        inputs->item(0, 1)->setText("B");
        click(view, "digital_evaluate");
        QVERIFY(status(view).contains("nonempty and unique"));
        inputs->item(0, 1)->setText(" π ");
        outputs(view)->item(0, 0)->setText(" π ");
        click(view, "digital_truth");
        QVERIFY(status(view).startsWith("Valid circuit"));
        auto* truth = widget<QTableWidget>(view, "digital_truth_table");
        QCOMPARE(truth->horizontalHeaderItem(0)->text(), QString("In:  π "));
        QCOMPARE(truth->horizontalHeaderItem(2)->text(), QString("Out:  π "));
        outputs(view)->item(1, 0)->setText(" π ");
        click(view, "digital_truth");
        QVERIFY(status(view).contains("nonempty and unique"));
        QCOMPARE(truth->rowCount(), 0);
        inputs->item(0, 1)->setText(
            QString(static_cast<int>(openece::digital::limits::name_bytes), QChar(0x03c0)));
        click(view, "digital_evaluate");
        QVERIFY(status(view).contains("byte limit"));
    }
    void guiResourceLimits() {
        DigitalLogicView view;
        for (int i = 2; i < digital_limits::inputs; ++i)
            click(view, "digital_add_input");
        click(view, "digital_add_input");
        QCOMPARE(widget<QTableWidget>(view, "digital_inputs")->rowCount(), digital_limits::inputs);
        QVERIFY(status(view).contains("Input limit"));
        click(view, "digital_truth");
        QCOMPARE(widget<QTableWidget>(view, "digital_truth_table")->rowCount(),
                 1 << digital_limits::inputs);
        for (int i = 2; i < digital_limits::gates; ++i)
            click(view, "digital_add_gate");
        click(view, "digital_add_gate");
        QCOMPARE(gates(view)->rowCount(), digital_limits::gates);
        QVERIFY(status(view).contains("Gate limit"));
        for (int i = 2; i < digital_limits::outputs; ++i)
            click(view, "digital_add_output");
        click(view, "digital_add_output");
        QCOMPARE(outputs(view)->rowCount(), digital_limits::outputs);
        QVERIFY(status(view).contains("Output limit"));
        QCOMPARE(widget<QSpinBox>(view, "digital_pin_count")->maximum(), digital_limits::pins);
    }
    void emptyDraftIsEditableButCannotEvaluate() {
        DigitalLogicView view;
        for (int i = 0; i < 2; ++i) {
            widget<QTableWidget>(view, "digital_inputs")->selectRow(0);
            click(view, "digital_remove_input");
            gates(view)->selectRow(0);
            click(view, "digital_remove_gate");
            outputs(view)->selectRow(0);
            click(view, "digital_remove_output");
        }
        QVERIFY(!widget<QComboBox>(view, "digital_gate_kind")->isEnabled());
        click(view, "digital_truth");
        QVERIFY(status(view).contains("at least one primary input and output"));
        click(view, "digital_add_input");
        click(view, "digital_add_output");
        source(view, "digital_output_source_0", 5);
        click(view, "digital_evaluate");
        QVERIFY(status(view).startsWith("Valid circuit"));
    }
};
QTEST_MAIN(DigitalGuiTest)
#include "digital_gui_test.moc"
