#include "project_workflow.hpp"
#include "transient_view.hpp"
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPersistentModelIndex>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
using namespace openece;
using namespace openece::gui;
namespace {
template <class T> T* control(QObject& root, const char* name) {
    auto* p = root.findChild<T*>(name);
    if (!p)
        qFatal("Missing %s", name);
    return p;
}
project::ProjectSnapshot configured() {
    auto p = project::default_project();
    auto& t = p.circuits.transient;
    p.selected_domain = "circuits";
    p.circuits.selected_tab = "transient";
    t.nodes = {{{0}, "Ground π"}, {{4}, "<output>"}};
    t.next_node = {5};
    t.next_component = {8};
    t.ground = project::Id{99};
    t.components = {{{7},
                     "V 🚀",
                     "voltage_source",
                     project::Id{4},
                     project::Id{55},
                     {" 1e-\t", "mV"},
                     {"constant", {{{"", "ms"}, " NaN "}, {{"-2", "s"}, ""}}}}};
    t.probes = {{"I", "current", project::Id{5}, project::Id{6}, project::Id{8}},
                {"V", "voltage", project::Id{80}, std::nullopt, project::Id{88}}};
    t.initial_conditions = {{project::Id{9}, "capacitor_voltage", {"?", "uV"}},
                            {std::nullopt, "inductor_current", {"", "mA"}}};
    t.stop = {" 1e- ", "ns"};
    t.maximum_step = {"", "us"};
    return p;
}
void inert(ProjectWorkspace& w) {
    auto* v = control<TransientView>(w, "transient_view");
    QCOMPARE(v->findChildren<QTimer*>().size(), 0);
    for (auto* timer : w.findChildren<QTimer*>())
        QVERIFY(!timer->isActive());
    for (auto name : {"digital_truth_table", "timing_results", "circuit_voltages", "ac_voltages",
                      "ac_sweep_results", "comm_bits", "comm_ber_results"})
        QCOMPARE(control<QTableWidget>(w, name)->rowCount(), 0);
    QCOMPARE(control<QLabel>(w, "transient_status")->text(),
             QString("Editor only — no simulation results."));
}
QByteArray bytes(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("read failed");
    return f.readAll();
}
class Dialogs final : public ProjectDialogs {
  public:
    std::optional<QString> destination;
    SchemaUpgradeChoice upgrade = SchemaUpgradeChoice::cancel;
    UnsavedChoice unsaved_choice = UnsavedChoice::cancel;
    int warnings = 0, errors = 0;
    QString warned_path;
    std::function<void()> warning_hook;
    std::optional<QString> choose_open(const QString&) override { return destination; }
    std::optional<QString> choose_save(const QString&) override { return destination; }
    bool overwrite(const QString&) override { return true; }
    UnsavedChoice unsaved(const QString&) override { return unsaved_choice; }
    SchemaUpgradeChoice schema_upgrade(const QString& p) override {
        ++warnings;
        warned_path = p;
        if (warning_hook)
            warning_hook();
        return upgrade;
    }
    void error(const ProjectFailure&) override { ++errors; }
    void information(const QString&, const QString&) override {}
};
class Preferences final : public ProjectPreferences {
  public:
    QStringList paths;
    QStringList read_recent() override { return paths; }
    bool write_recent(const QStringList& p) override {
        paths = p;
        return true;
    }
};
class FailureWriter final : public ProjectWriter {
    std::unique_ptr<ProjectWriter> real_;
    int& mode_;

  public:
    FailureWriter(std::unique_ptr<ProjectWriter> r, int& m) : real_(std::move(r)), mode_(m) {}
    bool open() override { return mode_ != 1 && real_->open(); }
    qint64 write(const char* p, qint64 n) override {
        if (mode_ == 2)
            return -1;
        if (mode_ == 4) {
            mode_ = 2;
            return real_->write(p, std::min<qint64>(n, 3));
        }
        return real_->write(p, n);
    }
    bool failed() const override { return real_->failed(); }
    bool commit() override { return mode_ != 3 && real_->commit(); }
    QString error() const override { return "Injected failure"; }
};
class FailureIo final : public ProjectFileIo {
  public:
    int mode = 0;
    std::unique_ptr<ProjectReader> reader(const QString& p) override {
        return qt_project_file_io()->reader(p);
    }
    std::unique_ptr<ProjectWriter> writer(const QString& p) override {
        return std::make_unique<FailureWriter>(qt_project_file_io()->writer(p), mode);
    }
};
void open(ProjectDocument& d, const QString& path) {
    auto p = d.prepare_open(path);
    if (!std::holds_alternative<PreparedProject>(p))
        qFatal("prepare failed");
    QVERIFY(
        std::holds_alternative<std::monostate>(d.install(std::get<PreparedProject>(std::move(p)))));
}
void old_file(const QString& path) {
    QFile source(QString(OPENECE_SOURCE_DIR) + "/tests/fixtures/v0.9/incomplete.openece");
    QVERIFY(source.open(QIODevice::ReadOnly));
    QFile output(path);
    QVERIFY(output.open(QIODevice::WriteOnly));
    auto data = source.readAll();
    QCOMPARE(output.write(data), data.size());
}
void pending(QTableWidget* t, int row, int column, const QString& text) {
    t->setCurrentCell(row, column);
    t->setFocus();
    QTest::keyClick(t, Qt::Key_F2);
    QCoreApplication::processEvents();
    auto* e = qobject_cast<QLineEdit*>(QApplication::focusWidget());
    QVERIFY(e);
    e->selectAll();
    QTest::keyClicks(e, text);
    QCOMPARE(e->text(), text);
}
} // namespace
class TransientGuiTest final : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void inertRoundTrips() {
        for (auto p : {project::default_project(), configured()}) {
            ProjectWorkspace w(p, true);
            inert(w);
            QCOMPARE(w.capture(), p);
            ProjectWorkspace fresh(
                project::decode_project(project::encode_project(w.capture())).snapshot, true);
            inert(fresh);
            QCOMPARE(fresh.capture(), p);
        }
        for (auto tab : {"editor", "sources", "initial_conditions", "probes", "help"}) {
            auto p = configured();
            p.circuits.transient.selected_tab = tab;
            TransientView view(nullptr, &p.circuits.transient);
            view.synchronize_pending_text();
            QCOMPARE(p.circuits.transient.selected_tab, std::string(tab));
        }
    }
    void navigationAndRawText() {
        ProjectDocument d(std::make_unique<ProjectWorkspace>(configured(), true));
        auto& w = d.workspace();
        w.resize(1100, 800);
        w.show();
        auto* stop = control<QLineEdit>(w, "transient_stop");
        QVERIFY(!d.dirty());
        stop->setFocus();
        stop->selectAll();
        QTest::keyClicks(stop, " 1e- "); // identical => clean
        QTest::keyClicks(stop, "x");
        QVERIFY(d.dirty());
        auto* tabs = control<QTabWidget>(w, "transient_tabs");
        tabs->setCurrentIndex(1);
        auto* points = control<QTableWidget>(w, "transient_points");
        pending(points, 0, 2, " 1e- ");
        control<QListWidget>(w, "domain_navigation")->setCurrentRow(3);
        auto p = w.capture();
        QCOMPARE(p.circuits.transient.components[0].source.points[0].value_text,
                 std::string(" 1e- "));
        QCOMPARE(p.circuits.transient.stop.text, std::string(" 1e- x"));
        ProjectDocument clean(std::make_unique<ProjectWorkspace>(configured(), true));
        control<QTabWidget>(clean.workspace(), "transient_tabs")->setCurrentIndex(3);
        QVERIFY(!clean.dirty());
    }
    void atomicUnitsAndKindRejection() {
        auto p = configured();
        auto& c = p.circuits.transient.components[0];
        c.value = {"2", "V"};
        c.source.points = {{{"1", "ms"}, "3"}, {{"2", "s"}, "1e-"}};
        TransientView view(nullptr, &p.circuits.transient);
        QSignalSpy edits(&view, &DraftView::draftEdited);
        auto before = p.circuits.transient;
        auto* components = control<QTableWidget>(view, "transient_components");
        auto* u = static_cast<QComboBox*>(components->cellWidget(0, 6));
        u->setCurrentText("mV");
        QCOMPARE(p.circuits.transient, before);
        QCOMPARE(edits.size(), 0);
        QCOMPARE(u->currentText(), QString("V"));
        auto* kind = static_cast<QComboBox*>(components->cellWidget(0, 2));
        kind->setCurrentText("current_source");
        QCOMPARE(p.circuits.transient, before);
        QCOMPARE(edits.size(), 0);
        control<QTableWidget>(view, "transient_points")->item(1, 2)->setText("4");
        edits.clear();
        u->setCurrentText("mV");
        QCOMPARE(c.value, (project::Quantity{"2000", "mV"}));
        QCOMPARE(c.source.points[0].value_text, std::string("3000"));
        QCOMPARE(c.source.points[1].value_text, std::string("4000"));
        QCOMPARE(edits.size(), 1);
        u->setCurrentText("uV");
        QCOMPARE(c.value.text, std::string("2000000"));
        components->item(0, 5)->setText("1e308");
        auto copy = p.circuits.transient;
        u->setCurrentText("V"); // smaller numeric value is representable
        QCOMPARE(c.value.unit, std::string("V"));
        copy = p.circuits.transient;
        components->item(0, 5)->setText("1e308");
        copy = p.circuits.transient;
        u->setCurrentText("uV");
        QCOMPARE(p.circuits.transient, copy);
        components->item(0, 5)->setText("1");
        u->setCurrentText("uV");
        QCOMPARE(c.value.unit, std::string("uV"));
        components->item(0, 5)->setText("1e-320");
        copy = p.circuits.transient;
        u->setCurrentText("V");
        QCOMPARE(p.circuits.transient, copy); // nonzero amplitude would underflow to zero
    }
    void quantityUnitsAndRetainedInitialConditions() {
        auto p = configured();
        p.circuits.transient.stop = {"2", "s"};
        p.circuits.transient.initial_conditions[1].value = {"3", "A"};
        TransientView v(nullptr, &p.circuits.transient);
        control<QComboBox>(v, "transient_stop_unit")->setCurrentText("ms");
        QCOMPARE(p.circuits.transient.stop, (project::Quantity{"2000", "ms"}));
        auto* t = control<QTableWidget>(v, "transient_initial_conditions");
        static_cast<QComboBox*>(t->cellWidget(1, 3))->setCurrentText("mA");
        QCOMPARE(p.circuits.transient.initial_conditions[1].value,
                 (project::Quantity{"3000", "mA"}));
        control<QComboBox>(v, "transient_initialization")->setCurrentIndex(1);
        control<QComboBox>(v, "transient_initialization")->setCurrentIndex(0);
        QCOMPARE(p.circuits.transient.initial_conditions.size(), 2U);
        auto* pts = control<QTableWidget>(v, "transient_points");
        pts->item(0, 0)->setText("2");
        static_cast<QComboBox*>(pts->cellWidget(0, 1))->setCurrentText("us");
        QCOMPARE(p.circuits.transient.components[0].source.points[0].time,
                 (project::Quantity{"2000", "us"}));
    }
    void idsRemovalAndExhaustion() {
        auto p = configured();
        TransientView v(nullptr, &p.circuits.transient);
        control<QPushButton>(v, "transient_add_node")->click();
        QCOMPARE(p.circuits.transient.nodes.back().id, project::Id{7});
        control<QPushButton>(v, "transient_add_component")->click();
        QCOMPARE(p.circuits.transient.components.back().id, project::Id{10});
        control<QTableWidget>(v, "transient_nodes")->setCurrentCell(1, 1);
        control<QPushButton>(v, "transient_remove_node")->click();
        QCOMPARE(p.circuits.transient.components[0].positive, project::Reference{project::Id{4}});
        control<QTableWidget>(v, "transient_components")->setCurrentCell(0, 1);
        control<QPushButton>(v, "transient_remove_component")->click();
        QCOMPARE(p.circuits.transient.probes[0].component, project::Reference{project::Id{8}});
        p.circuits.transient.next_node = {project::limits::exhausted_id};
        auto before = p.circuits.transient;
        control<QPushButton>(v, "transient_add_node")->click();
        QCOMPARE(p.circuits.transient, before);
    }
    void exhaustedAllocationDirtyTracksCounterChange() {
        auto p = project::default_project();
        auto& t = p.circuits.transient;
        t.next_node = {project::limits::exhausted_id - 1};
        t.next_component = {project::limits::exhausted_id - 1};
        t.probes = {{"reserved", "current", project::Id{0xffffffffU}, std::nullopt,
                     project::Id{0xffffffffU}}};
        ProjectDocument d(std::make_unique<ProjectWorkspace>(p, true));
        QVERIFY(!d.dirty());
        QSignalSpy edits(&d.workspace(), &DraftView::draftEdited);
        control<QPushButton>(d.workspace(), "transient_add_node")->click();
        QCOMPARE(d.workspace().capture().circuits.transient.next_node.value,
                 project::limits::exhausted_id);
        QVERIFY(d.dirty());
        QCOMPARE(edits.size(), 1);
        control<QPushButton>(d.workspace(), "transient_add_component")->click();
        QCOMPARE(d.workspace().capture().circuits.transient.next_component.value,
                 project::limits::exhausted_id);
        QCOMPARE(edits.size(), 2);
        control<QPushButton>(d.workspace(), "transient_add_node")->click();
        control<QPushButton>(d.workspace(), "transient_add_component")->click();
        QCOMPARE(edits.size(), 2); // Already exhausted: no additional persisted edit.
        QVERIFY(d.workspace().capture().circuits.transient.nodes.empty());
        QVERIFY(d.workspace().capture().circuits.transient.components.empty());
    }
    void pendingSaveOpenNewClose() {
        QTemporaryDir dir;
        auto destination = dir.filePath("project π space.openece");
        auto dialogs = std::make_shared<Dialogs>();
        dialogs->destination = destination;
        auto prefs = std::make_shared<Preferences>();
        ProjectDocument d(std::make_unique<ProjectWorkspace>(configured(), true));
        ProjectWorkflow workflow(d, dialogs, prefs);
        auto& w = d.workspace();
        w.resize(1200, 800);
        w.show();
        pending(control<QTableWidget>(w, "transient_components"), 0, 5, " 1e- ");
        QVERIFY(workflow.save_as());
        QVERIFY(!d.dirty());
        auto expected = w.capture();
        QVERIFY(workflow.open_path(destination));
        QCOMPARE(d.workspace().capture(), expected);
        inert(d.workspace());
        d.workspace().resize(1200, 800);
        d.workspace().show();
        pending(control<QTableWidget>(d.workspace(), "transient_components"), 0, 5, " 2e- ");
        dialogs->unsaved_choice = UnsavedChoice::save;
        QVERIFY(workflow.open_path(destination)); // Save active delegate, then re-stage same file.
        QCOMPARE(d.workspace().capture().circuits.transient.components[0].value.text,
                 std::string(" 2e- "));
        QVERIFY(!d.dirty());
        control<QLineEdit>(d.workspace(), "transient_stop")->setText(" pending π ");
        dialogs->unsaved_choice = UnsavedChoice::save;
        QVERIFY(workflow.new_project());
        auto loaded = ProjectFileStore{}.load(destination);
        QCOMPARE(std::get<project::DecodedProject>(loaded).snapshot.circuits.transient.stop.text,
                 std::string(" pending π "));
        QVERIFY(workflow.open_path(destination));
        control<QLineEdit>(d.workspace(), "transient_step")->setText(" 1e- ");
        QVERIFY(workflow.request_close());
        loaded = ProjectFileStore{}.load(destination);
        QCOMPARE(
            std::get<project::DecodedProject>(loaded).snapshot.circuits.transient.maximum_step.text,
            std::string(" 1e- "));
    }
    void provenanceWarningsCancelSaveAsAndAliases() {
        QTemporaryDir dir;
        auto path = dir.filePath("old π project.openece");
        old_file(path);
        auto original = bytes(path);
        ProjectDocument d;
        open(d, path);
        QCOMPARE(d.source_schema_version(), 1);
        auto dialogs = std::make_shared<Dialogs>();
        auto prefs = std::make_shared<Preferences>();
        ProjectWorkflow flow(d, dialogs, prefs);
        QVERIFY(!flow.save());
        QCOMPARE(dialogs->warnings, 1);
        QCOMPARE(bytes(path), original);
        QCOMPARE(d.source_schema_version(), 1);
        dialogs->destination = path;
        QVERIFY(!flow.save_as());
        QCOMPARE(dialogs->warnings, 2);
        QCOMPARE(bytes(path), original);
        dialogs->upgrade = SchemaUpgradeChoice::save_as;
        dialogs->destination = dir.filePath("new copy.openece");
        QVERIFY(flow.save());
        QCOMPARE(d.source_schema_version(), 2);
        QCOMPARE(bytes(path), original);
        QCOMPARE(std::get<project::DecodedProject>(ProjectFileStore{}.load(d.path()))
                     .source_schema_version,
                 2);
        open(d, path);
        dialogs->upgrade = SchemaUpgradeChoice::overwrite;
        QVERIFY(flow.save());
        QCOMPARE(d.source_schema_version(), 2);
        QVERIFY(bytes(path) != original);
        QVERIFY(flow.save());
        QCOMPARE(dialogs->warnings, 4); // no repeated warning after committed upgrade
        old_file(path);
        open(d, path);
        dialogs->destination = dir.path() + "/./old π project.openece";
        dialogs->upgrade = SchemaUpgradeChoice::cancel;
        QVERIFY(!flow.save_as());
        QVERIFY(d.requires_schema_upgrade(*dialogs->destination));
        QCOMPARE(bytes(path), original);
    }
    void sameFileUpgradeRestagesAfterSave() {
        QTemporaryDir dir;
        const auto path = dir.filePath("original.openece");
        old_file(path);
        ProjectDocument d;
        open(d, path);
        auto dialogs = std::make_shared<Dialogs>();
        auto prefs = std::make_shared<Preferences>();
        dialogs->upgrade = SchemaUpgradeChoice::overwrite;
        dialogs->unsaved_choice = UnsavedChoice::save;
        ProjectWorkflow workflow(d, dialogs, prefs);
        control<QLineEdit>(d.workspace(), "transient_stop")->setText(" pending π ");
        QVERIFY(workflow.open_path(path));
        QCOMPARE(d.workspace().capture().circuits.transient.stop.text, std::string(" pending π "));
        QCOMPARE(d.source_schema_version(), 2);
        QVERIFY(!d.dirty());
        QCOMPARE(dialogs->warnings, 1);
    }
    void failedLoadAndRestoreKeepCompleteSession() {
        QTemporaryDir dir;
        const auto path = dir.filePath("candidate.openece");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        const auto encoded = QByteArray::fromStdString(project::encode_project(configured()));
        QCOMPARE(f.write(encoded), encoded.size());
        f.close();
        ProjectDocument d(std::make_unique<ProjectWorkspace>(configured(), true),
                          std::make_shared<ProjectFileStore>(), [](project::ProjectSnapshot p) {
                              p.circuits.transient.stop.text = "silently changed";
                              return std::make_unique<ProjectWorkspace>(p, true);
                          });
        control<QLineEdit>(d.workspace(), "transient_step")->setText("unfinished");
        const auto before = d.workspace().capture();
        const auto revision = d.revision();
        auto staged = d.prepare_open(path);
        QVERIFY(std::holds_alternative<ProjectFailure>(staged));
        QCOMPARE(std::get<ProjectFailure>(staged).code, project::ErrorCode::restore_failed);
        QCOMPARE(d.workspace().capture(), before);
        QCOMPARE(d.revision(), revision);
        QVERIFY(d.dirty());
        QCOMPARE(d.source_schema_version(), 2);
        QVERIFY(d.path().isEmpty());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"format\":\"org.openece.project\",\"schema_version\":2}");
        f.close();
        staged = d.prepare_open(path);
        QVERIFY(std::holds_alternative<ProjectFailure>(staged));
        QCOMPARE(d.workspace().capture(), before);
        QCOMPARE(d.revision(), revision);
        QVERIFY(d.dirty());
        QCOMPARE(d.source_schema_version(), 2);
    }
    void failedUpgradeKeepsPathProvenanceDestinationAndDirty() {
        QTemporaryDir dir;
        auto path = dir.filePath("old.openece");
        old_file(path);
        auto original = bytes(path);
        auto io = std::make_shared<FailureIo>();
        ProjectDocument d(std::make_unique<ProjectWorkspace>(),
                          std::make_shared<ProjectFileStore>(io));
        open(d, path);
        control<QLineEdit>(d.workspace(), "transient_stop")->setText("1e-");
        auto before = d.workspace().capture();
        for (int mode : {1, 2, 3, 4}) {
            io->mode = mode;
            auto result = d.save();
            QVERIFY(std::holds_alternative<ProjectFailure>(result));
            QCOMPARE(bytes(path), original);
            QCOMPARE(d.path(), absolute_project_path(path));
            QCOMPARE(d.source_schema_version(), 1);
            QVERIFY(d.dirty());
            QCOMPARE(d.workspace().capture(), before);
        }
        io->mode = 3;
        auto result = d.save_as(dir.filePath("copy.openece"));
        QVERIFY(std::holds_alternative<ProjectFailure>(result));
        QCOMPARE(d.source_schema_version(), 1);
        QCOMPARE(d.path(), absolute_project_path(path));
        io->mode = 0;
        QVERIFY(std::holds_alternative<SaveStatus>(d.save()));
        QCOMPARE(d.source_schema_version(), 2);
        QVERIFY(!d.dirty());
    }
};
QTEST_MAIN(TransientGuiTest)
#include "transient_gui_test.moc"
