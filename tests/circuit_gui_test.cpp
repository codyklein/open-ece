#include "circuit_value.hpp"
#include "circuits_view.hpp"
#include "main_window.hpp"
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QtTest>
#include <cmath>
using namespace openece::gui;
class CircuitGuiTest : public QObject {
    Q_OBJECT
    QTableWidget* table(QWidget& v, const char* name) { return v.findChild<QTableWidget*>(name); }
    QTableWidget* parts(QWidget& v) { return table(v, "circuit_components"); }
    QComboBox* select(QWidget& v, int row, int column) {
        return static_cast<QComboBox*>(parts(v)->cellWidget(row, column));
    }
    QLineEdit* value(QWidget& v, int row) {
        return static_cast<QLineEdit*>(parts(v)->cellWidget(row, 5));
    }
    void click(QWidget& v, const char* name) {
        v.findChild<QPushButton*>(name)->click();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    QString status(QWidget& v) { return v.findChild<QLabel*>("circuit_status")->text(); }
    void node(QWidget& v, int row, int column, unsigned id) {
        auto* c = select(v, row, column);
        c->setCurrentIndex(c->findData(id));
    }
  private Q_SLOTS:
    void dividerAndDomainPersistence() {
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* nav = window.findChild<QListWidget*>("domain_navigation");
        QCOMPARE(nav->count(), 4);
        nav->setCurrentRow(2);
        QVERIFY(status(window).startsWith("Solved DC"));
        auto* volts = table(window, "circuit_voltages");
        auto* amps = table(window, "circuit_currents");
        QCOMPARE(volts->rowCount(), 3);
        QCOMPARE(volts->item(2, 1)->text().toDouble(), 5.0);
        QCOMPARE(amps->item(0, 1)->text().toDouble(), -.005);
        value(window, 0)->setText("12");
        QCOMPARE(volts->rowCount(), 0);
        click(window, "circuit_solve");
        QCOMPARE(volts->item(2, 1)->text().toDouble(), 6.0);
        nav->setCurrentRow(0);
        nav->setCurrentRow(1);
        nav->setCurrentRow(2);
        QCOMPARE(value(window, 0)->text(), QString("12"));
        QCOMPARE(volts->item(2, 1)->text().toDouble(), 6.0);
        auto path = qEnvironmentVariable("OPENECE_CIRCUIT_SCREENSHOT");
        if (!path.isEmpty())
            QVERIFY(window.grab().save(path));
    }
    void unitsConvertPendingPhysicalValue() {
        CircuitsView v;
        value(v, 1)->setText("2000");
        select(v, 1, 6)->setCurrentIndex(1);
        QCOMPARE(value(v, 1)->text(), QString("2"));
        click(v, "circuit_solve");
        QVERIFY(status(v).startsWith("Solved DC"));
        QVERIFY(std::abs(table(v, "circuit_voltages")->item(2, 1)->text().toDouble() - 10.0 / 3) <
                1e-10);
        value(v, 1)->setText("3");
        select(v, 1, 6)->setCurrentIndex(0);
        QCOMPARE(value(v, 1)->text(), QString("3000"));
        value(v, 0)->setText(".01");
        select(v, 0, 6)->setCurrentIndex(1);
        QCOMPARE(value(v, 0)->text(), QString("10"));
        select(v, 0, 6)->setCurrentIndex(2);
        QCOMPARE(value(v, 0)->text(), QString("10000"));
        QCOMPARE(select(v, 0, 6)->currentText(), QString::fromUtf8("µV"));
    }
    void invalidPendingTextSurvivesUnitSwitchAndSolve() {
        CircuitsView v;
        value(v, 1)->setText("1e-");
        select(v, 1, 6)->setCurrentIndex(1);
        QCOMPARE(select(v, 1, 6)->currentIndex(), 0);
        QCOMPARE(value(v, 1)->text(), QString("1e-"));
        QVERIFY(status(v).contains("Invalid pending"));
        click(v, "circuit_solve");
        QVERIFY(status(v).contains("Invalid numeric"));
        QCOMPARE(table(v, "circuit_voltages")->rowCount(), 0);
        value(v, 1)->setText("1000");
        click(v, "circuit_solve");
        QVERIFY(status(v).startsWith("Solved DC"));
    }
    void typeChangeClearsValueAndSupportsCurrentSource() {
        CircuitsView v;
        select(v, 0, 2)->setCurrentIndex(2);
        QVERIFY(value(v, 0)->text().isEmpty());
        QCOMPARE(select(v, 0, 6)->currentText(), QString("A"));
        value(v, 0)->setText("-.005");
        click(v, "circuit_solve");
        QVERIFY(status(v).startsWith("Solved DC"));
        QCOMPARE(table(v, "circuit_currents")->rowCount(), 0);
        QCOMPARE(table(v, "circuit_voltages")->item(1, 1)->text().toDouble(), 10.0);
    }
    void missingReferencesRetainIdsAndRecover() {
        CircuitsView v;
        auto* nodes = table(v, "circuit_nodes");
        nodes->selectRow(2);
        click(v, "circuit_remove_node");
        QVERIFY(select(v, 1, 4)->currentText().contains("Missing node 2"));
        click(v, "circuit_add_node");
        QCOMPARE(nodes->item(2, 0)->text(), QString("3"));
        click(v, "circuit_solve");
        QVERIFY(status(v).contains("distinct existing terminals"));
        node(v, 1, 4, 3);
        node(v, 2, 3, 3);
        click(v, "circuit_solve");
        QVERIFY(status(v).startsWith("Solved DC"));
        QCOMPARE(table(v, "circuit_voltages")->item(2, 1)->text().toDouble(), 5.0);
        nodes->item(2, 1)->setText("π midpoint");
        QCOMPARE(table(v, "circuit_voltages")->rowCount(), 0);
        QVERIFY(select(v, 1, 4)->currentText().contains("π midpoint"));
        click(v, "circuit_solve");
        QVERIFY(status(v).startsWith("Solved DC"));
    }
    void missingGroundFloatingAndInvalidNames() {
        CircuitsView v;
        auto* ground = v.findChild<QComboBox*>("circuit_ground");
        ground->setCurrentIndex(0);
        click(v, "circuit_solve");
        QVERIFY(status(v).contains("ground"));
        ground->setCurrentIndex(ground->findData(0u));
        click(v, "circuit_add_node");
        click(v, "circuit_solve");
        QVERIFY(status(v).contains("Floating nodes"));
        table(v, "circuit_nodes")->selectRow(3);
        click(v, "circuit_remove_node");
        parts(v)->item(2, 1)->setText("R1");
        click(v, "circuit_solve");
        QVERIFY(status(v).contains("unique"));
        parts(v)->item(2, 1)->setText("R2");
        click(v, "circuit_solve");
        QVERIFY(status(v).startsWith("Solved DC"));
        ground->setCurrentIndex(ground->findData(2u));
        click(v, "circuit_solve");
        QCOMPARE(table(v, "circuit_voltages")->item(0, 1)->text().toDouble(), -5.0);
        QCOMPARE(table(v, "circuit_voltages")->item(2, 1)->text().toDouble(), 0.0);
    }
    void sourceLoopErrorsAndRecovery() {
        CircuitsView v;
        select(v, 1, 2)->setCurrentIndex(1);
        value(v, 1)->setText("5");
        select(v, 2, 2)->setCurrentIndex(1);
        value(v, 2)->setText("5");
        click(v, "circuit_solve");
        QVERIFY(status(v).contains("Redundant"));
        QCOMPARE(table(v, "circuit_currents")->rowCount(), 0);
        value(v, 2)->setText("6");
        click(v, "circuit_solve");
        QVERIFY(status(v).contains("Contradictory"));
        select(v, 2, 2)->setCurrentIndex(0);
        value(v, 2)->setText("1000");
        click(v, "circuit_solve");
        QVERIFY(status(v).startsWith("Solved DC"));
    }
    void newComponentsRequireExplicitConnections() {
        CircuitsView v;
        click(v, "circuit_add_component");
        value(v, 3)->setText("1000");
        click(v, "circuit_solve");
        QVERIFY(status(v).contains("Select both terminals"));
        node(v, 3, 3, 2);
        node(v, 3, 4, 0);
        click(v, "circuit_solve");
        QVERIFY(status(v).startsWith("Solved DC"));
        parts(v)->selectRow(3);
        click(v, "circuit_remove_component");
        click(v, "circuit_solve");
        QCOMPARE(table(v, "circuit_voltages")->item(2, 1)->text().toDouble(), 5.0);
    }
    void parserIsLocaleIndependentAndStrict() {
        struct Restore {
            QLocale previous;
            ~Restore() { QLocale::setDefault(previous); }
        } restore{QLocale()};
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
        QCOMPARE(circuit_value_si(" 1.25e3 ", 1e-3).value(), 1.25);
        QCOMPARE(circuit_value_si("-.5", 1).value(), -.5);
        for (const QString& text :
             {QString("1,5"), QString("1,000"), QString("nan"), QString("inf"), QString("1e"),
              QString(""), QString("1k"), QString("1e999"), QString("1e-999")})
            QVERIFY(!circuit_value_si(text, 1));
        QVERIFY(!circuit_value_si("1e308", 1e6));
        QVERIFY(!circuit_value_si("1e-320", 1e-6));
        QVERIFY(!circuit_value_si("1", 0));
    }
    void guiResourceLimits() {
        CircuitsView v;
        for (int i = 3; i <= circuit_gui_limits::nodes; ++i)
            click(v, "circuit_add_node");
        QCOMPARE(table(v, "circuit_nodes")->rowCount(), circuit_gui_limits::nodes);
        QVERIFY(status(v).contains("limit"));
        for (int i = 3; i <= circuit_gui_limits::components; ++i)
            click(v, "circuit_add_component");
        QCOMPARE(parts(v)->rowCount(), circuit_gui_limits::components);
        QVERIFY(status(v).contains("limit"));
        click(v, "circuit_example");
        QVERIFY(status(v).startsWith("Solved DC"));
    }
};
QTEST_MAIN(CircuitGuiTest)
#include "circuit_gui_test.moc"
