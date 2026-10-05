#include "main_window.hpp"
#include "project_workspace.hpp"
#include "replacement_test.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFocusFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>
using namespace openece::gui;
class UxDialogs final : public ProjectDialogs {
  public:
    QString path;
    int errors = 0;
    std::optional<QString> choose_open(const QString&) override { return path; }
    std::optional<QString> choose_save(const QString&) override { return path; }
    bool overwrite(const QString&) override { return true; }
    UnsavedChoice unsaved(const QString&) override { return UnsavedChoice::discard; }
    void error(const ProjectFailure&) override { ++errors; }
    void information(const QString&, const QString&) override {}
};
class UxPreferences final : public ProjectPreferences {
  public:
    QStringList paths;
    QStringList read_recent() override { return paths; }
    bool write_recent(const QStringList& p) override {
        paths = p;
        return true;
    }
};
class ReleaseUxTest : public QObject {
    Q_OBJECT
    template <class T> T* get(QObject& owner, const char* name) {
        auto* result = owner.findChild<T*>(name);
        Q_ASSERT(result);
        return result;
    }
    bool reach(QWidget* target) {
        for (int i = 0; i < 1200; ++i) {
            auto* focus = QApplication::focusWidget();
            if (focus == target || (focus && target->isAncestorOf(focus)) ||
                (qobject_cast<QAbstractSpinBox*>(focus) && target->parentWidget() == focus))
                return true;
            if (!focus)
                return false;
            QTest::keyClick(focus, Qt::Key_Tab);
        }
        qWarning() << "Cannot reach" << target->objectName() << "from"
                   << QApplication::focusWidget();
        return false;
    }
    bool press(QPushButton* button) {
        if (!reach(button))
            return false;
        QTest::keyClick(button, Qt::Key_Space);
        return true;
    }
    bool edit(QLineEdit* editor, const char* text) {
        if (!reach(editor))
            return false;
        QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClicks(editor, text);
        if (auto* spin = qobject_cast<QDoubleSpinBox*>(editor->parentWidget()))
            return static_cast<DraftDouble*>(spin)->raw_text() == QString(text);
        return editor->text() == QString(text);
    }
    bool tab(QTabWidget* tabs, int index) {
        if (!reach(tabs->tabBar()))
            return false;
        QTest::keyClick(tabs->tabBar(), Qt::Key_Home);
        // QTabBar uses arrows rather than Home on some platform styles.
        for (int i = 0; i < tabs->count(); ++i)
            QTest::keyClick(tabs->tabBar(), Qt::Key_Left);
        for (int i = 0; i < index; ++i)
            QTest::keyClick(tabs->tabBar(), Qt::Key_Right);
        return tabs->currentIndex() == index;
    }

  private Q_SLOTS:
    void keyboardDomainWorkflows() {
        ProjectWorkspace w(openece::project::default_project());
        w.resize(900, 650);
        w.show();
        w.activateWindow();
        QApplication::processEvents();
        auto* nav = get<QListWidget>(w, "domain_navigation");
        nav->setFocus();
        auto action = [&](const char* name) { return press(get<QPushButton>(w, name)); };
        auto domain = [&](int index) {
            if (!reach(nav))
                return false;
            QTest::keyClick(nav, Qt::Key_Home);
            for (int i = 0; i < index; ++i)
                QTest::keyClick(nav, Qt::Key_Down);
            return nav->currentRow() == index;
        };
        QVERIFY(edit(get<QDoubleSpinBox>(w, "frequency")->findChild<QLineEdit*>(), "20"));
        QVERIFY(action("generate"));
        QVERIFY(get<QLabel>(w, "status")->text().contains("complete"));
        QVERIFY(edit(get<QDoubleSpinBox>(w, "frequency")->findChild<QLineEdit*>(), "1e-"));
        QVERIFY(get<QLabel>(w, "status")->text().contains("stale"));
        QVERIFY(action("generate"));
        QVERIFY(get<QLabel>(w, "status")->text().contains("Failed"));
        QVERIFY(edit(get<QDoubleSpinBox>(w, "frequency")->findChild<QLineEdit*>(), "20"));
        QVERIFY(action("generate"));
        for (const char* name : {"spectral_window", "filter_type"}) {
            auto* box = get<QComboBox>(w, name);
            QVERIFY(reach(box));
            QTest::keyClick(box, Qt::Key_End);
        }
        QVERIFY(action("generate"));
        QVERIFY(domain(1));
        auto* inputs = get<QTableWidget>(w, "digital_inputs");
        QVERIFY(reach(inputs));
        QTest::keyClick(inputs, Qt::Key_Home, Qt::ControlModifier);
        QTest::keyClick(inputs, Qt::Key_Right);
        QTest::keyClick(inputs, Qt::Key_F2);
        QVERIFY(qobject_cast<QLineEdit*>(QApplication::focusWidget()));
        QTest::keyClicks(QApplication::focusWidget(), "renamed");
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
        QVERIFY(action("digital_evaluate"));
        QVERIFY(get<QLabel>(w, "digital_status")->text().contains("complete"));
        auto* toggle = inputs->findChild<QCheckBox*>();
        QVERIFY(toggle);
        QVERIFY(reach(toggle));
        QTest::keyClick(toggle, Qt::Key_Space);
        QVERIFY(action("digital_truth"));
        QVERIFY(tab(get<QTabWidget>(w, "digital_tabs"), 1));
        auto* clocks = get<QTableWidget>(w, "timing_inputs");
        QVERIFY(reach(clocks));
        QTest::keyClick(clocks, Qt::Key_Home, Qt::ControlModifier);
        for (int i = 0; i < 3; ++i)
            QTest::keyClick(clocks, Qt::Key_Right);
        QTest::keyClick(clocks, Qt::Key_F2);
        QVERIFY(qobject_cast<QLineEdit*>(QApplication::focusWidget()));
        QTest::keyClicks(QApplication::focusWidget(), "1000");
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
        QVERIFY(tab(get<QTabWidget>(w, "timing_editor_tabs"), 3));
        auto* stimuli = get<QTableWidget>(w, "timing_stimuli");
        QVERIFY(reach(stimuli));
        QTest::keyClick(stimuli, Qt::Key_F2);
        QVERIFY(qobject_cast<QLineEdit*>(QApplication::focusWidget()));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
        QVERIFY(action("timing_step"));
        QVERIFY(get<QLabel>(w, "timing_status")->text().contains("Paused"));
        QVERIFY(action("timing_run"));
        QVERIFY(action("timing_pause"));
        QVERIFY(get<QLabel>(w, "timing_status")->text().contains("Paused"));
        QVERIFY(action("timing_reset"));
        QCOMPARE(get<QTableWidget>(w, "timing_results")->rowCount(), 0);
        QVERIFY(domain(2));
        QVERIFY(tab(get<QTabWidget>(w, "circuits_analysis_tabs"), 0));
        auto* parts = get<QTableWidget>(w, "circuit_components");
        auto* value = qobject_cast<QLineEdit*>(parts->cellWidget(0, 5));
        QVERIFY(edit(value, "1e-"));
        QVERIFY(action("circuit_solve"));
        QVERIFY(get<QLabel>(w, "circuit_status")->text().contains("Failed"));
        QVERIFY(edit(value, "10"));
        QVERIFY(action("circuit_solve"));
        QVERIFY(get<QLabel>(w, "circuit_status")->text().contains("complete"));
        QVERIFY(reach(get<QComboBox>(w, "circuit_ground")));
        QVERIFY(reach(qobject_cast<QComboBox*>(parts->cellWidget(0, 3))));
        QVERIFY(tab(get<QTabWidget>(w, "circuits_analysis_tabs"), 1));
        QVERIFY(action("ac_solve"));
        QVERIFY(get<QLabel>(w, "ac_status")->text().contains("complete"));
        QVERIFY(tab(get<QTabWidget>(w, "ac_view_tabs"), 2));
        for (const char* name : {"ac_probe_positive", "ac_reference_source", "ac_spacing"})
            QVERIFY(reach(get<QComboBox>(w, name)));
        QVERIFY(action("ac_run_sweep"));
        // Deliver one normal timer batch deterministically; wall-clock waits can
        // finish the whole small sweep on a fast Release runner.
        QTimer* sweep_timer = nullptr;
        for (auto* parent = get<QPushButton>(w, "ac_run_sweep")->parentWidget();
             parent && !sweep_timer; parent = parent->parentWidget())
            sweep_timer = parent->findChild<QTimer*>();
        QVERIFY(sweep_timer && sweep_timer->isActive());
        QVERIFY(QMetaObject::invokeMethod(sweep_timer, "timeout", Qt::DirectConnection));
        QVERIFY(action("ac_cancel"));
        QVERIFY(get<QLabel>(w, "ac_status")->text().contains("cancelled"));
        auto* sweep = get<QTableWidget>(w, "ac_sweep_results");
        QVERIFY(sweep->rowCount() > 0);
        QCOMPARE(sweep->item(0, 5)->text(), QString("Accepted"));
        QVERIFY(sweep->item(sweep->rowCount() - 1, 5)->text().contains("cancelled"));
        QVERIFY(domain(3));
        QVERIFY(action("comm_simulate"));
        QVERIFY(get<QLabel>(w, "comm_status")->text().contains("complete"));
        auto* source = get<QComboBox>(w, "comm_source");
        QVERIFY(reach(source));
        QTest::keyClick(source, Qt::Key_End);
        QVERIFY(edit(get<QLineEdit>(w, "comm_manual"), "01"));
        QVERIFY(action("comm_simulate"));
        QVERIFY(tab(get<QTabWidget>(w, "comm_tabs"), 2));
        QVERIFY(action("comm_step"));
        QVERIFY(get<QLabel>(w, "comm_status")->text().contains("partial"));
        const auto count =
            get<QTableWidget>(w, "comm_ber_results")->item(0, 2)->data(Qt::UserRole).toULongLong();
        QVERIFY(action("comm_run"));
        QVERIFY(get<QLabel>(w, "comm_status")->text().contains("running"));
        QVERIFY(action("comm_cancel"));
        QVERIFY(get<QLabel>(w, "comm_status")->text().contains("cancelled"));
        QVERIFY(action("comm_step"));
        QVERIFY(
            get<QTableWidget>(w, "comm_ber_results")->item(0, 2)->data(Qt::UserRole).toULongLong() >
            count);
        QVERIFY(domain(0));
        QVERIFY(reach(get<QPushButton>(w, "generate")));
        auto* focused = QApplication::focusWidget();
        QTest::keyClick(focused, Qt::Key_Backtab);
        QVERIFY(QApplication::focusWidget() != focused);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), focused);
    }
    void currentFocusIndicatorFollowsKeyboard() {
        ProjectWorkspace w(openece::project::default_project());
        w.resize(1100, 850);
        w.show();
        w.activateWindow();
        QApplication::processEvents();
        auto* nav = get<QListWidget>(w, "domain_navigation");
        auto* indicator = get<QFocusFrame>(w, "keyboard_focus_indicator");
        nav->setFocus(Qt::TabFocusReason);
        QCOMPARE(indicator->widget(), QApplication::focusWidget());
        QTest::keyClick(nav, Qt::Key_Tab);
        QVERIFY(QApplication::focusWidget() != nav);
        QCOMPARE(indicator->widget(), QApplication::focusWidget());
        QVERIFY(indicator->isVisible());
        QVERIFY(indicator->style()->pixelMetric(QStyle::PM_FocusFrameHMargin) >= 5);
        QVERIFY(indicator->style()->styleHint(QStyle::SH_FocusFrame_AboveWidget));
        QVERIFY(!indicator->style()->styleHint(QStyle::SH_FocusFrame_Mask));
        QCOMPARE(nav->currentRow(), 0); // Selection stays, focus outline moves.
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Backtab);
        QCOMPARE(QApplication::focusWidget(), nav);
        QCOMPARE(indicator->widget(), nav);
        for (int row : {0, 1}) {
            nav->setCurrentRow(row);
            nav->setFocus(Qt::TabFocusReason);
            auto* target = row == 0 ? static_cast<QWidget*>(get<QPushButton>(w, "generate"))
                                    : static_cast<QWidget*>(get<QTableWidget>(w, "digital_inputs"));
            QVERIFY(reach(target));
            QCOMPARE(indicator->widget(), QApplication::focusWidget());
            QVERIFY(indicator->widget() != nav);
            QVERIFY(indicator->isVisible());
        }
        QVERIFY(tab(get<QTabWidget>(w, "digital_tabs"), 1));
        QVERIFY(reach(get<QPushButton>(w, "timing_step")));
        QCOMPARE(indicator->widget(), QApplication::focusWidget());
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Space);
        QVERIFY(get<QLabel>(w, "timing_status")->text().contains("Paused"));
        QCOMPARE(indicator->widget(), QApplication::focusWidget());
        if (auto dir = qEnvironmentVariable("OPENECE_UX_SCREENSHOTS"); !dir.isEmpty()) {
            QDir().mkpath(dir);
            w.grab().save(dir + "/current-focus.png");
        }
        QWidget outside;
        outside.show();
        outside.activateWindow();
        outside.setFocus();
        QApplication::processEvents();
        QVERIFY(!indicator->widget());
    }
    void inactiveNumericSelectionCannotMasqueradeAsFocus_data() {
        QTest::addColumn<QString>("frequency");
        QTest::newRow("valid") << QString("20");
        QTest::newRow("incomplete") << QString("1e-");
        QTest::newRow("empty") << QString("");
        QTest::newRow("whitespace") << QString(" 25 ");
    }
    void inactiveNumericSelectionCannotMasqueradeAsFocus() {
        QFETCH(QString, frequency);
        auto project = openece::project::default_project();
        project.signals.frequency.text = frequency.toUtf8().toStdString();
        ProjectWorkspace w(std::move(project), true);
        w.resize(1100, 850);
        w.show();
        w.activateWindow();
        QApplication::processEvents();
        auto* nav = get<QListWidget>(w, "domain_navigation");
        nav->setFocus(Qt::TabFocusReason);
        auto* indicator = get<QFocusFrame>(w, "keyboard_focus_indicator");
        const auto initial = w.capture().signals;
        QSignalSpy edits(&w, &DraftView::draftEdited);
        QList<QLineEdit*> visited;
        for (const auto* name : {"amplitude", "frequency", "sample_rate", "duration"}) {
            auto* spin = get<QDoubleSpinBox>(w, name);
            QVERIFY(reach(spin));
            auto* editor = spin->findChild<QLineEdit*>();
            QVERIFY(editor);
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_A, Qt::ControlModifier);
            // Spin-box Select All excludes its unit suffix. An empty numerical
            // buffer therefore has nothing to select even when "Hz" is visible.
            QCOMPARE(editor->hasSelectedText(),
                     QString::fromLatin1(name) != "frequency" || !frequency.isEmpty());
            QCOMPARE(indicator->widget(), QApplication::focusWidget());
            for (auto* previous : visited)
                QVERIFY(!previous->hasSelectedText());
            visited.append(editor);
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
            QVERIFY(!editor->hasSelectedText());
            QCOMPARE(indicator->widget(), QApplication::focusWidget());
            QApplication::processEvents();
            const auto image = indicator->grab().toImage();
            bool visible_blue_ring = false;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    visible_blue_ring |= image.pixelColor(x, y) == QColor(0, 85, 210);
            QVERIFY(visible_blue_ring);
        }
        if (auto dir = qEnvironmentVariable("OPENECE_UX_SCREENSHOTS"); !dir.isEmpty()) {
            QDir().mkpath(dir);
            w.grab().save(dir + "/numeric-focus.png");
        }
        QCOMPARE(w.capture().signals, initial); // Selection/focus never edits the draft.
        QCOMPARE(edits.count(), 0);
    }
    void projectShortcutsAndPrompts() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto dialogs = std::make_shared<UxDialogs>();
        dialogs->path = dir.path() + QString::fromUtf8("/project π space.openece");
        auto preferences = std::make_shared<UxPreferences>();
        auto document = std::make_unique<ProjectDocument>();
        auto* doc = document.get();
        MainWindow w(std::move(document), dialogs, preferences);
        w.show();
        w.activateWindow();
        QApplication::processEvents();
        get<QListWidget>(w, "domain_navigation")->setFocus();
        QVERIFY(edit(get<QDoubleSpinBox>(w, "frequency")->findChild<QLineEdit*>(), "1e-"));
        auto shortcut = [&](Qt::Key key, Qt::KeyboardModifiers modifiers) {
            QTest::keyClick(QApplication::focusWidget(), key, modifiers);
            QApplication::processEvents();
        };
        shortcut(Qt::Key_S, Qt::ControlModifier);
        QCOMPARE(doc->path(), dialogs->path);
        QVERIFY(!doc->dirty());
        QCOMPARE(doc->workspace().capture().signals.frequency.text, std::string("1e-"));
        shortcut(Qt::Key_N, Qt::ControlModifier);
        QVERIFY(doc->path().isEmpty());
        shortcut(Qt::Key_O, Qt::ControlModifier);
        QCOMPARE(doc->workspace().capture().signals.frequency.text, std::string("1e-"));
        dialogs->path = dir.path() + "/second";
        shortcut(Qt::Key_S, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(doc->path().endsWith("second.openece"));
        QCOMPARE(dialogs->errors, 0);
        QVERIFY(preferences->paths.size() == 2);
        QTest::keyClick(&w, Qt::Key_F, Qt::AltModifier);
        auto* file = w.menuBar()->actions().first()->menu();
        QTRY_VERIFY(file->isVisible());
        auto* recent = get<QMenu>(w, "project_recent");
        for (int i = 0; i < 12 && (!file->activeAction() || file->activeAction()->menu() != recent);
             ++i)
            QTest::keyClick(file, Qt::Key_Down);
        QVERIFY(file->activeAction() && file->activeAction()->menu() == recent);
        QTest::keyClick(file, Qt::Key_Right);
        QTRY_VERIFY(recent->isVisible());
        QTest::keyClick(recent, Qt::Key_Down);
        QTest::keyClick(recent, Qt::Key_Return);
        QApplication::processEvents();
        QVERIFY(!doc->dirty());
        for (auto choice : {QMessageBox::Save, QMessageBox::Discard, QMessageBox::Cancel}) {
            bool reached = false;
            QTimer::singleShot(0, &w, [&] {
                auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if (!box)
                    return;
                QTest::qWait(10); // Let the window system assign the modal dialog's initial focus.
                reached = reach(box->button(choice));
                if (reached)
                    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Space);
                else
                    box->reject();
            });
            auto response = qt_project_dialogs(&w)->unsaved("Keyboard test");
            QVERIFY(reached);
            QCOMPARE(response, choice == QMessageBox::Save      ? UnsavedChoice::save
                               : choice == QMessageBox::Discard ? UnsavedChoice::discard
                                                                : UnsavedChoice::cancel);
        }
    }
    void replacementCancelAndAccept() {
        ProjectWorkspace w(openece::project::default_project());
        for (const char* name : {"circuit_example", "ac_example_rc", "ac_example_rlc",
                                 "timing_example", "digital_copy_timing"}) {
            const auto before = w.capture();
            bool seen = false;
            QTimer::singleShot(0, &w, [&] {
                auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if (!box)
                    return;
                seen = box->defaultButton() == box->button(QMessageBox::Cancel);
                QTest::keyClick(box, Qt::Key_Escape);
            });
            get<QPushButton>(w, name)->click();
            QVERIFY(seen);
            QVERIFY(w.capture() == before);
            accept_replacement(w);
            get<QPushButton>(w, name)->click();
        }
    }

    void signalEnlargedTextFits_data() {
        QTest::addColumn<int>("points");
        QTest::newRow("18-point") << 18;
        QTest::newRow("24-point") << 24;
    }
    void signalEnlargedTextFits() {
        QFETCH(int, points);
        ProjectWorkspace w(openece::project::default_project());
        auto font = w.font();
        font.setPointSize(points);
        w.setFont(font);
        w.resize(1280, 900);
        w.show();
        QApplication::processEvents();
        for (const char* text : {"Sample &rate", "Spectral &window"}) {
            QLabel* label = nullptr;
            for (auto* candidate : w.findChildren<QLabel*>())
                if (candidate->text() == QString(text))
                    label = candidate;
            QVERIFY(label);
            QVERIFY2(label->width() >= label->sizeHint().width(), qPrintable(label->text()));
        }
        for (auto name : {"phase_unit", "spectral_window", "filter_type"}) {
            auto* box = get<QComboBox>(w, name);
            QVERIFY2(box->width() >= box->minimumSizeHint().width(), name);
        }
        auto* pi = get<QPushButton>(w, "insert_pi");
        QVERIFY(pi->width() >= pi->sizeHint().width());
        if (auto dir = qEnvironmentVariable("OPENECE_UX_SCREENSHOTS"); !dir.isEmpty()) {
            QDir().mkpath(dir);
            w.grab().save(dir + "/signals-font-" + QString::number(points) + ".png");
        }
    }
    void smallWindow_data() {
        QTest::addColumn<int>("points");
        QTest::newRow("normal") << 0;
        QTest::newRow("enlarged-text") << 18;
    }
    void smallWindow() {
        QFETCH(int, points);
        ProjectWorkspace w(openece::project::default_project());
        if (points) {
            auto f = w.font();
            f.setPointSize(points);
            w.setFont(f);
        }
        w.resize(900, 650);
        w.show();
        QApplication::processEvents();
        auto* navigation = get<QListWidget>(w, "domain_navigation");
        QVERIFY(navigation->viewport()->width() >=
                navigation->fontMetrics().horizontalAdvance("Communications"));
        qInfo() << "requested 900x650, actual" << w.size() << "minimum" << w.minimumSizeHint();
        get<QPushButton>(w, "generate")->click();
        w.resize(800, 600);
        QApplication::processEvents();
        QCOMPARE(w.size(), QSize(800, 600));
        if (auto dir = qEnvironmentVariable("OPENECE_UX_SCREENSHOTS"); !dir.isEmpty()) {
            QDir().mkpath(dir);
            auto* nav = w.findChild<QListWidget*>("domain_navigation");
            for (int i = 0; i < nav->count(); ++i) {
                nav->setCurrentRow(i);
                QApplication::processEvents();
                w.grab().save(dir + "/domain-" + QString::number(i) + ".png");
            }
        }
        QVERIFY2(w.width() <= 900 && w.height() <= 650,
                 "Workbench forces a larger-than-requested window");
    }
};
QTEST_MAIN(ReleaseUxTest)
#include "release_ux_test.moc"
