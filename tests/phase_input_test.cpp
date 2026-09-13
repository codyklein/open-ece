#include "phase_input.hpp"

#include <QtTest>

#include <cmath>
#include <numbers>
#include <stdexcept>

using openece::gui::format_phase;
using openece::gui::parse_phase;

class PhaseInputTest : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void piExpressions_data() {
        QTest::addColumn<QString>("input");
        QTest::addColumn<double>("degrees");
        QTest::newRow("pi") << QString("pi") << 180.0;
        QTest::newRow("half") << QString("pi/2") << 90.0;
        QTest::newRow("three quarters") << QString("3*pi/4") << 135.0;
        QTest::newRow("negative") << QString("-pi/2") << -90.0;
        QTest::newRow("full turn") << QString("2*pi") << 360.0;
        QTest::newRow("negative full turn") << QString("-2*pi") << -360.0;
        QTest::newRow("unicode") << QString("π") << 180.0;
        QTest::newRow("unicode half") << QString("π/2") << 90.0;
        QTest::newRow("implicit unicode") << QString("3π/4") << 135.0;
        QTest::newRow("implicit ascii") << QString("3pi/4") << 135.0;
        QTest::newRow("whitespace and case") << QString(" \t- 3 * PI / 4 \n") << -135.0;
        QTest::newRow("decimal coefficient") << QString(".5*pi") << 90.0;
        QTest::newRow("decimal divisor") << QString("pi/0.5") << 360.0;
        QTest::newRow("scientific notation") << QString("+5e-1*pi") << 90.0;
    }

    void piExpressions() {
        QFETCH(QString, input);
        QFETCH(double, degrees);
        const double radians = parse_phase(input, true);
        QVERIFY(std::abs(radians - degrees / 180.0 * std::numbers::pi) < 1e-14);
        QVERIFY(std::abs(format_phase(radians, false).toDouble() - degrees) < 1e-12);
    }

    void unicodeAndLocaleIndependentParsing() {
        // Explicit UTF-8 bytes independently check the compiler's source encoding.
        const QString pi = QString::fromUtf8("\xCF\x80");
        QCOMPARE(QString("π"), pi);
        QCOMPARE(parse_phase(pi + "/2", true), std::numbers::pi / 2.0);
        const QLocale previous;
        struct RestoreLocale {
            QLocale locale;
            ~RestoreLocale() { QLocale::setDefault(locale); }
        } restore{previous};
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
        QCOMPARE(parse_phase("1.25", true), 1.25);
        QCOMPARE(parse_phase(".5*pi", true), std::numbers::pi / 2.0);
        QCOMPARE(format_phase(1.25, true), QString("1.25"));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, (void)parse_phase("1,25", true));
    }

    void decimalsAndRoundTrips() {
        QCOMPARE(parse_phase(" 1.25 ", true), 1.25);
        QCOMPARE(parse_phase(" - .5 ", true), -0.5);
        QCOMPARE(parse_phase("1e-3", true), 0.001);
        QVERIFY(std::abs(parse_phase("90.5", false) - 90.5 / 180.0 * std::numbers::pi) < 1e-14);
        for (double angle : {0.0, 0.1234567890123456, -1.5, std::numbers::pi,
                             2.0 * std::numbers::pi, -2.0 * std::numbers::pi}) {
            QCOMPARE(parse_phase(format_phase(angle, true), true), angle);
            double converted = angle;
            for (int i = 0; i < 20; ++i) {
                converted = parse_phase(format_phase(converted, false), false);
                converted = parse_phase(format_phase(converted, true), true);
            }
            QVERIFY(std::abs(converted - angle) < 1e-13);
        }
    }

    void rejectsMalformedExpressions() {
        for (const QString& text :
             {QString(""),        QString(" "),      QString("pi/"),      QString("pi/0"),
              QString("pi/-2"),   QString("pi/+2"),  QString("pi+1"),     QString("(pi)"),
              QString("sin(pi)"), QString("2**pi"),  QString("*pi"),      QString("pi*2"),
              QString("pi/pi"),   QString("pi/2/3"), QString("--pi"),     QString("p i"),
              QString("1 2"),     QString("1 .5"),   QString("1e -2"),    QString("2/3"),
              QString("NaN"),     QString("inf"),    QString("1,5"),      QString("90°"),
              QString("1 rad"),   QString("1e999"),  QString("1e999*pi"), QString("pi/1e999")}) {
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument, (void)parse_phase(text, true));
        }
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, (void)parse_phase("pi", false));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, (void)parse_phase(QString(129, ' '), true));
    }

    void rejectsOutOfRangeWithoutWrapping() {
        for (const QString& text :
             {QString("3*pi"), QString("-3π"), QString("7"), QString("1e308*pi/1e-308")}) {
            QVERIFY_THROWS_EXCEPTION(std::invalid_argument, (void)parse_phase(text, true));
        }
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, (void)parse_phase("360.1", false));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, (void)parse_phase("-361", false));
    }
};

QTEST_GUILESS_MAIN(PhaseInputTest)
#include "phase_input_test.moc"
