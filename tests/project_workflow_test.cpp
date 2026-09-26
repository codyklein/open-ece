#include "main_window.hpp"
#include <QAction>
#include <QFile>
#include <QListWidget>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
using namespace openece;
using namespace openece::gui;
namespace {
class Dialogs final : public ProjectDialogs {
  public:
    std::optional<QString> open_path, save_path;
    UnsavedChoice choice = UnsavedChoice::cancel;
    bool confirm = true;
    int prompts = 0, opens = 0, saves = 0, confirms = 0;
    QString confirmed;
    std::vector<ProjectFailure> errors;
    QStringList messages;
    std::function<void()> on_prompt;
    std::optional<QString> choose_open(const QString&) override {
        ++opens;
        return open_path;
    }
    std::optional<QString> choose_save(const QString&) override {
        ++saves;
        return save_path;
    }
    bool overwrite(const QString& p) override {
        ++confirms;
        confirmed = p;
        return confirm;
    }
    UnsavedChoice unsaved(const QString&) override {
        ++prompts;
        if (on_prompt)
            on_prompt();
        return choice;
    }
    void error(const ProjectFailure& e) override { errors.push_back(e); }
    void information(const QString&, const QString& message) override {
        messages.push_back(message);
    }
};
class Preferences final : public ProjectPreferences {
  public:
    QStringList paths;
    bool success = true;
    int writes = 0;
    QStringList read_recent() override { return paths; }
    bool write_recent(const QStringList& p) override {
        ++writes;
        paths = p;
        return success;
    }
};
class FailWriter final : public ProjectWriter {
    std::unique_ptr<ProjectWriter> real_;
    const bool& fail_;

  public:
    FailWriter(std::unique_ptr<ProjectWriter> real, const bool& fail)
        : real_(std::move(real)), fail_(fail) {}
    bool open() override { return real_->open(); }
    qint64 write(const char* p, qint64 n) override { return real_->write(p, n); }
    bool failed() const override { return real_->failed(); }
    bool commit() override { return !fail_ && real_->commit(); }
    QString error() const override { return "Injected commit failure"; }
};
class Io final : public ProjectFileIo {
  public:
    bool fail = false;
    std::unique_ptr<ProjectReader> reader(const QString& p) override {
        return qt_project_file_io()->reader(p);
    }
    std::unique_ptr<ProjectWriter> writer(const QString& p) override {
        return std::make_unique<FailWriter>(qt_project_file_io()->writer(p), fail);
    }
};
struct Fixture {
    QTemporaryDir dir;
    std::shared_ptr<Dialogs> dialogs = std::make_shared<Dialogs>();
    std::shared_ptr<Preferences> prefs = std::make_shared<Preferences>();
    std::shared_ptr<Io> io = std::make_shared<Io>();
    ProjectDocument document{std::make_unique<ProjectWorkspace>(),
                             std::make_shared<ProjectFileStore>(io)};
    ProjectWorkflow workflow{document, dialogs, prefs};
    void edit(const QString& text = " 1e-π ") {
        document.workspace().findChild<QLineEdit*>("phase")->setText(text);
    }
    QString file(const QString& name = "project.openece") { return dir.filePath(name); }
    void create(const QString& path) {
        auto p = project::default_project();
        p.signals.phase.text = "loaded";
        if (std::holds_alternative<ProjectFailure>(ProjectFileStore{}.save(p, path)))
            qFatal("Cannot write fixture");
    }
};
project::ProjectSnapshot loaded(const QString& p) {
    auto result = ProjectFileStore{}.load(p);
    if (auto* e = std::get_if<ProjectFailure>(&result))
        qFatal("%s", qPrintable(e->message));
    return std::get<project::DecodedProject>(result).snapshot;
}
QByteArray bytes(const QString& p) {
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("Cannot read fixture");
    return f.readAll();
}
void inert(ProjectWorkspace& w) {
    for (auto* t : w.findChildren<QTimer*>())
        QVERIFY(!t->isActive());
    for (auto name : {"digital_truth_table", "timing_results", "circuit_voltages", "ac_voltages",
                      "ac_sweep_results", "comm_bits", "comm_ber_results"})
        QCOMPARE(w.findChild<QTableWidget*>(name)->rowCount(), 0);
}
} // namespace
class ProjectWorkflowTest final : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void replacementDecisions_data() {
        QTest::addColumn<QString>("operation");
        QTest::addColumn<QString>("decision");
        for (auto op : {"new", "open", "close"})
            for (auto choice :
                 {"clean", "save", "discard", "cancel", "save-failed", "save-cancelled"})
                QTest::newRow(qPrintable(QString(op) + "-" + choice))
                    << QString(op) << QString(choice);
    }
    void replacementDecisions() {
        QFETCH(QString, operation);
        QFETCH(QString, decision);
        Fixture f;
        f.create(f.file("open.openece"));
        f.dialogs->open_path = f.file("open.openece");
        f.dialogs->save_path = f.file("saved.openece");
        if (decision != "clean")
            f.edit();
        if (decision.startsWith("save"))
            f.dialogs->choice = UnsavedChoice::save;
        if (decision == "discard")
            f.dialogs->choice = UnsavedChoice::discard;
        if (decision == "save-failed")
            f.io->fail = true;
        if (decision == "save-cancelled")
            f.dialogs->save_path.reset();
        auto before = f.document.workspace().capture();
        auto revision = f.document.revision();
        QPointer<ProjectWorkspace> old = &f.document.workspace();
        const bool ok = operation == "new"    ? f.workflow.new_project()
                        : operation == "open" ? f.workflow.open_project()
                                              : f.workflow.request_close();
        const bool expected = decision == "clean" || decision == "save" || decision == "discard";
        QCOMPARE(ok, expected);
        QCOMPARE(f.dialogs->prompts, decision == "clean" ? 0 : 1);
        if (!ok) {
            QCOMPARE(&f.document.workspace(), old.data());
            QVERIFY(f.document.workspace().capture() == before);
            QCOMPARE(f.document.revision(), revision);
            QVERIFY(f.document.path().isEmpty());
            QVERIFY(f.document.dirty());
            QVERIFY(f.prefs->paths.empty());
        } else if (operation != "close") {
            QVERIFY(old.isNull());
            QVERIFY(!f.document.dirty());
            inert(f.document.workspace());
            if (operation == "new") {
                QVERIFY(f.document.workspace().capture() == project::default_project());
                QVERIFY(f.document.path().isEmpty());
            } else {
                QCOMPARE(f.document.path(), f.file("open.openece"));
                QCOMPARE(f.document.workspace().capture().signals.phase.text,
                         std::string("loaded"));
            }
        }
        if (decision == "save")
            QVERIFY(loaded(f.file("saved.openece")) == before);
        if (decision == "save-failed") {
            QCOMPARE(f.dialogs->errors.size(), std::size_t{1});
            QCOMPARE(f.dialogs->errors[0].code, project::ErrorCode::save_commit_failed);
        }
    }
    void saveAsAndExistingSave() {
        Fixture f;
        f.edit();
        f.dialogs->save_path = f.file("space π 名");
        QVERIFY(f.workflow.save());
        const auto target = f.file("space π 名.openece");
        QCOMPARE(f.document.path(), target);
        QVERIFY(!f.document.dirty());
        QCOMPARE(f.dialogs->saves, 1);
        QCOMPARE(f.prefs->paths, QStringList{target});
        QCOMPARE(loaded(target).signals.phase.text, std::string(" 1e-π "));
        f.edit("second");
        QVERIFY(f.workflow.save());
        QCOMPARE(f.dialogs->saves, 1);
        QCOMPARE(f.dialogs->confirms, 0); // Normal Save already targets the current project.
        QCOMPARE(loaded(target).signals.phase.text, std::string("second"));
        const auto old_bytes = bytes(target);
        f.edit("third");
        f.io->fail = true;
        QVERIFY(!f.workflow.save());
        QCOMPARE(bytes(target), old_bytes);
        QVERIFY(f.document.dirty());
        const auto history = f.prefs->paths;
        f.dialogs->save_path = f.file("failed.openece");
        QVERIFY(!f.workflow.save_as());
        QCOMPARE(f.document.path(), target);
        QCOMPARE(f.prefs->paths, history);
        f.io->fail = false;
        f.dialogs->save_path.reset();
        const auto revision = f.document.revision();
        QVERIFY(!f.workflow.save_as());
        QCOMPARE(f.document.path(), target);
        QCOMPARE(f.document.revision(), revision);
        f.dialogs->save_path = f.file("new.openece");
        QVERIFY(f.workflow.save_as());
        QCOMPARE(f.document.path(), f.file("new.openece"));
        QCOMPARE(f.prefs->paths.front(), f.file("new.openece"));
        QCOMPARE(bytes(target), old_bytes);
    }
    void extensionAndOverwrite() {
        QCOMPARE(project_save_destination("a"), QString("a.openece"));
        QCOMPARE(project_save_destination("a.txt"), QString("a.txt.openece"));
        QCOMPARE(project_save_destination("a.OPENECE"), QString("a.OPENECE"));
        Fixture f;
        f.create(f.file());
        const auto previous = bytes(f.file());
        f.edit();
        f.dialogs->save_path = f.file("project");
        f.dialogs->confirm = false;
        QVERIFY(!f.workflow.save_as());
        QCOMPARE(f.dialogs->confirmed, f.file());
        QCOMPARE(bytes(f.file()), previous);
        QVERIFY(f.document.path().isEmpty());
        QVERIFY(f.document.dirty());
        QVERIFY(f.prefs->paths.empty());
        f.dialogs->confirm = true;
        QVERIFY(f.workflow.save_as());
        QVERIFY(bytes(f.file()) != previous);
        QCOMPARE(f.document.path(), f.file());
    }
    void openFailureCancellationAndUnknownFields() {
        Fixture f;
        f.edit();
        const auto before = f.document.workspace().capture();
        const auto revision = f.document.revision();
        auto* old = &f.document.workspace();
        QVERIFY(!f.workflow.open_project()); // cancelled picker
        QVERIFY(!f.workflow.open_path(f.file("missing.openece")));
        QCOMPARE(f.dialogs->errors.back().code, project::ErrorCode::read_failed);
        QFile bad(f.file());
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("{");
        bad.close();
        QVERIFY(!f.workflow.open_path(f.file()));
        QCOMPARE(f.dialogs->errors.back().code, project::ErrorCode::malformed_json);
        QCOMPARE(f.dialogs->prompts, 0); // Stage failure precedes unsaved-change resolution.
        QCOMPARE(&f.document.workspace(), old);
        QCOMPARE(f.document.revision(), revision);
        QVERIFY(f.document.workspace().capture() == before);
        QVERIFY(f.prefs->paths.empty());
        auto text = QByteArray::fromStdString(project::encode_project(project::default_project()));
        text.insert(1, "\"future_note\":1,");
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write(text);
        bad.close();
        f.dialogs->choice = UnsavedChoice::discard;
        QVERIFY(f.workflow.open_path(f.file()));
        QCOMPARE(f.dialogs->messages.size(), 1);
        QVERIFY(f.dialogs->messages[0].contains("discarded"));
        QVERIFY(!f.document.dirty());
    }
    void sameFileOpenRestages_data() {
        QTest::addColumn<bool>("named");
        QTest::newRow("save-existing") << true;
        QTest::newRow("save-as-alias") << false;
    }
    void sameFileOpenRestages() {
        QFETCH(bool, named);
        Fixture f;
        f.create(f.file());
        if (named)
            QVERIFY(f.workflow.open_path(f.file()));
        f.edit("must not be lost");
        const auto expected = f.document.workspace().capture();
        f.dialogs->choice = UnsavedChoice::save;
        f.dialogs->save_path = f.dir.path() + "/./project.openece";
        QVERIFY(f.workflow.open_path(f.file()));
        QVERIFY(f.document.workspace().capture() == expected);
        QVERIFY(!f.document.dirty());
        inert(f.document.workspace());
        QCOMPARE(f.prefs->paths.size(), 1);
    }
    void activeEditorWorkflows_data() {
        QTest::addColumn<QString>("operation");
        QTest::addColumn<bool>("delegate");
        for (auto op : {"save", "save-as", "new", "open", "close"})
            for (bool delegate : {false, true})
                QTest::newRow(qPrintable(QString(op) + (delegate ? "-delegate" : "-line")))
                    << QString(op) << delegate;
    }
    void activeEditorWorkflows() {
        QFETCH(QString, operation);
        QFETCH(bool, delegate);
        Fixture f;
        auto initial = project::default_project();
        initial.selected_domain = "digital";
        initial.digital.selected_tab = "timing";
        auto prepared = f.document.prepare_new(initial, DocumentState::clean);
        QVERIFY(std::holds_alternative<PreparedProject>(prepared));
        auto installed = f.document.install(std::move(std::get<PreparedProject>(prepared)));
        QVERIFY(std::holds_alternative<std::monostate>(installed));
        f.dialogs->save_path = f.file("saved.openece");
        if (operation == "save")
            QVERIFY(f.workflow.save());
        f.document.workspace().show();
        QLineEdit* active = nullptr;
        if (delegate) {
            auto* table = f.document.workspace().findChild<QTableWidget*>("timing_inputs");
            table->editItem(table->item(0, 3));
            QCoreApplication::processEvents();
            for (auto* e : table->findChildren<QLineEdit*>())
                if (e->property("draftIndex").isValid())
                    active = e;
        } else
            active = f.document.workspace().findChild<QLineEdit*>("phase");
        QVERIFY(active);
        active->setText(" 1e-π "); // delegate hasn't committed to the table item
        f.dialogs->choice = UnsavedChoice::save;
        f.create(f.file("open.openece"));
        const bool ok = operation == "save"      ? f.workflow.save()
                        : operation == "save-as" ? f.workflow.save_as()
                        : operation == "new"     ? f.workflow.new_project()
                        : operation == "open"    ? f.workflow.open_path(f.file("open.openece"))
                                                 : f.workflow.request_close();
        QVERIFY(ok);
        const auto snapshot = loaded(f.file("saved.openece"));
        QCOMPARE(delegate ? snapshot.digital.timing.inputs[0].clock.first_edge_text
                          : snapshot.signals.phase.text,
                 std::string(" 1e-π "));
    }
    void recentOrderingLimitsMissingRemovalAndSettings() {
        Fixture f;
        for (int i = 0; i < 12; ++i) {
            f.dialogs->save_path = f.file(QString("π %1.openece").arg(i));
            QVERIFY(f.workflow.save_as());
        }
        QCOMPARE(f.prefs->paths.size(), 10);
        QCOMPARE(f.prefs->paths.front(), f.file("π 11.openece"));
        QVERIFY(f.workflow.open_path(f.file("π 5.openece")));
        QCOMPARE(f.prefs->paths.front(), f.file("π 5.openece"));
        f.dialogs->save_path = f.dir.path() + "/./π 5.openece";
        QVERIFY(f.workflow.save_as());
        QCOMPARE(f.prefs->paths.size(), 10);
        const auto history = f.prefs->paths;
        QVERIFY(QFile::remove(history[0]));
        QVERIFY(!f.workflow.open_path(history[0]));
        QCOMPARE(f.prefs->paths, history);
        f.workflow.remove_recent(history[0]);
        QCOMPARE(f.prefs->paths.size(), 9);
        const auto settings_path = f.file("preferences.ini");
        {
            SettingsProjectPreferences prefs(
                std::make_unique<QSettings>(settings_path, QSettings::IniFormat));
            QVERIFY(prefs.write_recent(f.prefs->paths));
        }
        auto prefs = std::make_shared<SettingsProjectPreferences>(
            std::make_unique<QSettings>(settings_path, QSettings::IniFormat));
        ProjectDocument d;
        ProjectWorkflow w(d, f.dialogs, prefs);
        QCOMPARE(w.recent_projects(), f.prefs->paths);
        QVERIFY(d.path().isEmpty());
        QVERIFY(!d.dirty()); // Never reopen on startup.
        QVERIFY(d.workspace().capture() == project::default_project());
    }
    void preferencesFailureDoesNotFailSaveAndReentryIsBlocked() {
        Fixture f;
        f.prefs->success = false;
        f.edit();
        f.dialogs->save_path = f.file();
        QVERIFY(f.workflow.save());
        QVERIFY(!f.document.dirty());
        QCOMPARE(f.document.path(), f.file());
        QCOMPARE(f.dialogs->messages.size(), 1);
        f.edit("changed");
        f.dialogs->choice = UnsavedChoice::cancel;
        f.dialogs->on_prompt = [&] {
            QVERIFY(!f.workflow.new_project());
            QVERIFY(!f.workflow.save());
            QVERIFY(!f.workflow.request_close());
        };
        QVERIFY(!f.workflow.new_project());
        QVERIFY(f.document.dirty());
    }
    void windowActionsTitlesAndCloseVeto() {
        QTemporaryDir tmp;
        auto dialogs = std::make_shared<Dialogs>();
        auto prefs = std::make_shared<Preferences>();
        auto io = std::make_shared<Io>();
        MainWindow w(std::make_unique<ProjectDocument>(std::make_unique<ProjectWorkspace>(),
                                                       std::make_shared<ProjectFileStore>(io)),
                     dialogs, prefs);
        w.show();
        QCOMPARE(w.windowTitle(), QString("Untitled — OpenECE"));
        auto* phase = w.findChild<QLineEdit*>("phase");
        phase->setText("1e-");
        QCOMPARE(w.windowTitle(), QString("Untitled* — OpenECE"));
        dialogs->choice = UnsavedChoice::cancel;
        QVERIFY(!w.close());
        QVERIFY(w.isVisible());
        dialogs->choice = UnsavedChoice::save;
        QVERIFY(!w.close()); // cancelled Save As
        dialogs->save_path = tmp.filePath("named π.openece");
        w.findChild<QAction*>("project_save")->trigger();
        QCOMPARE(w.windowTitle(), QString("named π.openece — OpenECE"));
        QCOMPARE(w.toolTip(), *dialogs->save_path);
        QCOMPARE(w.statusBar()->currentMessage(), *dialogs->save_path);
        phase->setText("2");
        QCOMPARE(w.windowTitle(), QString("named π.openece* — OpenECE"));
        io->fail = true;
        QVERIFY(!w.close());
        QVERIFY(w.isVisible());
        io->fail = false;
        QVERIFY(w.close());
        QVERIFY(!w.document().dirty());
        for (auto* t : w.document().workspace().findChildren<QTimer*>())
            QVERIFY(!t->isActive());
        for (auto name : {"project_new", "project_open", "project_save", "project_save_as",
                          "project_close", "project_exit"}) {
            auto* a = w.findChild<QAction*>(name);
            QVERIFY(a);
            const auto key = QString(name) == "project_new"       ? QKeySequence::New
                             : QString(name) == "project_open"    ? QKeySequence::Open
                             : QString(name) == "project_save"    ? QKeySequence::Save
                             : QString(name) == "project_save_as" ? QKeySequence::SaveAs
                             : QString(name) == "project_close"   ? QKeySequence::Close
                                                                  : QKeySequence::Quit;
            QCOMPARE(a->shortcuts(), QKeySequence::keyBindings(key));
        }
    }
    void windowSessionReplacementAndRecentOpen() {
        QTemporaryDir tmp;
        auto dialogs = std::make_shared<Dialogs>();
        auto prefs = std::make_shared<Preferences>();
        MainWindow w(std::make_unique<ProjectDocument>(), dialogs, prefs);
        w.show();
        w.findChild<QLineEdit*>("phase")->setText("saved draft");
        dialogs->save_path = tmp.filePath("saved.openece");
        w.findChild<QAction*>("project_save_as")->trigger();
        QPointer<ProjectWorkspace> old = &w.document().workspace();
        w.findChild<QAction*>("project_new")->trigger();
        QVERIFY(old.isNull());
        QCOMPARE(w.centralWidget(), &w.document().workspace());
        QCOMPARE(w.windowTitle(), QString("Untitled — OpenECE"));
        QVERIFY(w.document().path().isEmpty());
        QVERIFY(w.document().workspace().capture() == project::default_project());
        QCOMPARE(w.workflow().recent_projects(), QStringList{*dialogs->save_path});
        auto* menu = w.findChild<QMenu*>("project_recent");
        QVERIFY(QMetaObject::invokeMethod(menu, "aboutToShow"));
        old = &w.document().workspace();
        menu->actions()[0]->trigger();
        QVERIFY(old.isNull());
        QCOMPARE(w.centralWidget(), &w.document().workspace());
        QCOMPARE(w.document().workspace().draft().signals.phase.text, std::string("saved draft"));
        QVERIFY(!w.document().dirty());
        inert(w.document().workspace());
        dialogs->open_path = tmp.filePath("missing.openece");
        old = &w.document().workspace();
        w.findChild<QAction*>("project_open")->trigger();
        QCOMPARE(&w.document().workspace(), old.data());
        QCOMPARE(dialogs->errors.size(), std::size_t{1});
    }
    void resultNavigationAndUserEdits() {
        Fixture f;
        f.document.workspace().findChild<QPushButton*>("generate")->click();
        QVERIFY(!f.document.dirty());
        auto* nav = f.document.workspace().findChild<QListWidget*>("domain_navigation");
        nav->setCurrentRow(2);
        QVERIFY(f.document.dirty());
        f.dialogs->save_path = f.file();
        QVERIFY(f.workflow.save());
        auto* tabs = f.document.workspace().findChild<QTabWidget*>("circuits_analysis_tabs");
        QVERIFY(tabs);
        tabs->setCurrentIndex(1);
        QVERIFY(f.document.dirty());
        QVERIFY(f.workflow.save());
        f.document.workspace().findChild<QPushButton*>("ac_solve")->click();
        QVERIFY(!f.document.dirty());
        QVERIFY(f.workflow.open_path(f.file()));
        QVERIFY(!f.document.dirty());
        inert(f.document.workspace());
    }
    void newAndCloseStopExecution() {
        Fixture f;
        auto* timer = f.document.workspace().findChild<QTimer*>();
        QVERIFY(timer);
        timer->start(100000);
        QVERIFY(timer->isActive());
        QPointer<QTimer> old(timer);
        QVERIFY(f.workflow.new_project());
        QVERIFY(old.isNull());
        inert(f.document.workspace());
        timer = f.document.workspace().findChild<QTimer*>();
        timer->start(100000);
        f.edit();
        f.dialogs->choice = UnsavedChoice::cancel;
        QVERIFY(!f.workflow.request_close());
        QVERIFY(timer->isActive());
        f.dialogs->choice = UnsavedChoice::discard;
        QVERIFY(f.workflow.request_close());
        QVERIFY(!timer->isActive());
    }
    void realExecutionReplacement_data() {
        QTest::addColumn<QString>("button");
        QTest::addColumn<bool>("closing");
        for (auto name : {"timing_run", "ac_run_sweep", "comm_run"})
            for (bool closing : {false, true})
                QTest::newRow(qPrintable(QString(name) + (closing ? "-close" : "-new")))
                    << QString(name) << closing;
    }
    void realExecutionReplacement() {
        QFETCH(QString, button);
        QFETCH(bool, closing);
        Fixture f;
        f.document.workspace().findChild<QPushButton*>(button)->click();
        QList<QPointer<QTimer>> active;
        for (auto* timer : f.document.workspace().findChildren<QTimer*>())
            if (timer->isActive())
                active.push_back(timer);
        QVERIFY(!active.empty());
        QVERIFY(!f.document.dirty());
        if (closing) {
            QVERIFY(f.workflow.request_close());
            for (const auto& timer : active)
                QVERIFY(timer && !timer->isActive());
        } else {
            QVERIFY(f.workflow.new_project());
            for (const auto& timer : active)
                QVERIFY(timer.isNull());
            inert(f.document.workspace());
        }
        QCOMPARE(f.dialogs->prompts, 0);
    }
    void missingRecentMenuIsActionable() {
        QTemporaryDir tmp;
        auto prefs = std::make_shared<Preferences>();
        prefs->paths = {tmp.filePath("missing π.openece")};
        auto dialogs = std::make_shared<Dialogs>();
        MainWindow w(std::make_unique<ProjectDocument>(), dialogs, prefs);
        auto* menu = w.findChild<QMenu*>("project_recent");
        QVERIFY(menu);
        QVERIFY(QMetaObject::invokeMethod(menu, "aboutToShow"));
        QVERIFY(menu->actions()[0]->text().contains("(missing)"));
        menu->actions()[0]->trigger();
        QCOMPARE(dialogs->errors.size(), std::size_t{1});
        QCOMPARE(w.workflow().recent_projects(), prefs->paths);
        QVERIFY(w.document().path().isEmpty());
        QVERIFY(!w.document().dirty());
        auto* remove = menu->actions().back()->menu();
        QVERIFY(remove);
        remove->actions()[0]->trigger();
        QVERIFY(w.workflow().recent_projects().empty());
    }
};
QTEST_MAIN(ProjectWorkflowTest)
#include "project_workflow_test.moc"
