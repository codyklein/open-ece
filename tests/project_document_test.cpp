#include "project_document.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <algorithm>
#include <cstring>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
using namespace openece;
using namespace openece::gui;
using Code = project::ErrorCode;
namespace {
template <class T> T& accepted(ProjectResult<T>& result) {
    if (auto* e = std::get_if<ProjectFailure>(&result))
        qFatal("Unexpected failure: %s (%s)", qPrintable(e->message), qPrintable(e->detail));
    return std::get<T>(result);
}
template <class T> const ProjectFailure& rejected(const ProjectResult<T>& result) {
    auto* e = std::get_if<ProjectFailure>(&result);
    if (!e)
        qFatal("Expected structured failure");
    return *e;
}
void write_file(const QString& path, const QByteArray& bytes) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size())
        qFatal("Cannot create fixture");
}
QByteArray read_file(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("Cannot read fixture");
    return f.readAll();
}
void edit(ProjectDocument& d, const QString& text) {
    d.workspace().findChild<QLineEdit*>("phase")->setText(text);
}
void inert(ProjectWorkspace& w) {
    for (auto* timer : w.findChildren<QTimer*>())
        QVERIFY(!timer->isActive());
    for (const char* name : {"digital_truth_table", "timing_results", "circuit_voltages",
                             "ac_voltages", "ac_sweep_results", "comm_bits", "comm_ber_results"})
        QCOMPARE(w.findChild<QTableWidget*>(name)->rowCount(), 0);
    for (auto* plot : w.findChildren<QwtPlot*>())
        for (auto* item : plot->itemList(QwtPlotItem::Rtti_PlotCurve))
            QCOMPARE(static_cast<QwtPlotCurve*>(item)->dataSize(), std::size_t{0});
}
enum class ReadFault { none, open, read, oversized, short_read, size_change, growth };
enum class WriteFault {
    none,
    open,
    write,
    short_then_fail,
    error_after_write,
    commit,
    exception,
    short_success
};
struct Faults {
    ReadFault read = ReadFault::none;
    WriteFault write = WriteFault::none;
    int reads = 0, opens = 0, writes = 0, commits = 0;
};
class FaultReader final : public ProjectReader {
    std::unique_ptr<ProjectReader> real_;
    std::shared_ptr<Faults> f_;

  public:
    FaultReader(std::unique_ptr<ProjectReader> real, std::shared_ptr<Faults> f)
        : real_(std::move(real)), f_(std::move(f)) {}
    bool open() override { return f_->read != ReadFault::open && real_->open(); }
    qint64 size() const override {
        if (f_->read == ReadFault::oversized)
            return static_cast<qint64>(project::limits::file_bytes + 1);
        if (f_->read == ReadFault::growth)
            return 1;
        if (f_->read == ReadFault::size_change && f_->reads > 0)
            return real_->size() + 1;
        return real_->size();
    }
    qint64 read(char* data, qint64 size) override {
        ++f_->reads;
        if (f_->read == ReadFault::read)
            return -1;
        if (f_->read == ReadFault::short_read)
            return f_->reads == 1 ? real_->read(data, std::min(size, qint64{3})) : 0;
        return real_->read(data, size);
    }
    bool failed() const override { return real_->failed(); }
    QString error() const override { return "Injected read failure"; }
};
class FaultWriter final : public ProjectWriter {
    std::unique_ptr<ProjectWriter> real_;
    std::shared_ptr<Faults> f_;

  public:
    FaultWriter(std::unique_ptr<ProjectWriter> real, std::shared_ptr<Faults> f)
        : real_(std::move(real)), f_(std::move(f)) {}
    bool open() override {
        ++f_->opens;
        return f_->write != WriteFault::open && real_->open();
    }
    qint64 write(const char* bytes, qint64 size) override {
        ++f_->writes;
        if (f_->write == WriteFault::write ||
            (f_->write == WriteFault::short_then_fail && f_->writes > 1))
            return -1;
        if (f_->write == WriteFault::short_then_fail || f_->write == WriteFault::short_success)
            size = std::min(size, qint64{31});
        return real_->write(bytes, size);
    }
    bool failed() const override {
        return real_->failed() || (f_->write == WriteFault::error_after_write && f_->writes > 0);
    }
    bool commit() override {
        ++f_->commits;
        if (f_->write == WriteFault::exception)
            throw std::runtime_error("internal exception must not escape");
        // Real QSaveFile has received the new bytes, but is never committed.
        // Destroying the underlying writer discards that temporary file.
        return f_->write != WriteFault::commit && real_->commit();
    }
    QString error() const override { return "Injected save failure"; }
};
class FaultIo final : public ProjectFileIo {
    std::shared_ptr<ProjectFileIo> real_ = qt_project_file_io();

  public:
    std::shared_ptr<Faults> faults = std::make_shared<Faults>();
    std::unique_ptr<ProjectReader> reader(const QString& path) override {
        return std::make_unique<FaultReader>(real_->reader(path), faults);
    }
    std::unique_ptr<ProjectWriter> writer(const QString& path) override {
        return std::make_unique<FaultWriter>(real_->writer(path), faults);
    }
};
} // namespace
class ProjectDocumentTest : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void realCrossDomainRoundTripAndOverwrite() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(QDir(tmp.path()).mkpath("nested π space"));
        auto path = tmp.filePath("nested π space/工程 🚀.openece");
        const auto bytes = read_file(QString::fromUtf8(OPENECE_PROJECT_FIXTURE));
        const auto snapshot =
            project::decode_project(
                std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())))
                .snapshot;
        ProjectFileStore store;
        auto saved = store.save(snapshot, path);
        accepted(saved);
        ProjectDocument document;
        auto candidate = document.prepare_open(path);
        QVERIFY(accepted(candidate).snapshot() == snapshot);
        QVERIFY(document.path().isEmpty());
        QSignalSpy replaced(&document, &ProjectDocument::workspaceReplaced);
        auto installed = document.install(std::move(accepted(candidate)));
        accepted(installed);
        QCOMPARE(replaced.count(), 1);
        QVERIFY(document.workspace().capture() == snapshot);
        QVERIFY(!document.dirty());
        inert(document.workspace());
        const auto second = tmp.filePath("second.openece");
        auto again = document.save_as(second);
        QCOMPARE(accepted(again), SaveStatus::saved);
        auto a = store.load(path), b = store.load(second);
        QVERIFY(accepted(a).snapshot == accepted(b).snapshot);
        edit(document, " 1e-π ");
        auto updated = document.save();
        QCOMPARE(accepted(updated), SaveStatus::saved);
        auto load = store.load(second);
        QVERIFY(accepted(load).snapshot == document.workspace().capture());
        QCOMPARE(accepted(load).snapshot.signals.phase.text, std::string(" 1e-π "));
    }
    void injectedSaveFailures_data() {
        QTest::addColumn<int>("fault");
        QTest::addColumn<int>("code");
        for (auto [fault, code] : {std::pair{WriteFault::open, Code::save_open_failed},
                                   {WriteFault::write, Code::save_write_failed},
                                   {WriteFault::short_then_fail, Code::save_write_failed},
                                   {WriteFault::error_after_write, Code::save_write_failed},
                                   {WriteFault::commit, Code::save_commit_failed},
                                   {WriteFault::exception, Code::save_commit_failed}})
            QTest::newRow(qPrintable(QString::number(static_cast<int>(fault))))
                << static_cast<int>(fault) << static_cast<int>(code);
    }
    void injectedSaveFailures() {
        QFETCH(int, fault);
        QFETCH(int, code);
        QTemporaryDir tmp;
        const auto old_path = tmp.filePath("old.openece"), target = tmp.filePath("target.openece");
        auto io = std::make_shared<FaultIo>();
        ProjectDocument d(std::make_unique<ProjectWorkspace>(),
                          std::make_shared<ProjectFileStore>(io));
        auto first = d.save_as(old_path);
        accepted(first);
        write_file(target, "previous destination bytes");
        edit(d, "1e-");
        const auto snapshot = d.workspace().capture();
        const auto revision = d.revision(), saved_revision = d.saved_revision();
        const auto files = QDir(tmp.path()).entryList(QDir::Files | QDir::Hidden);
        io->faults->write = static_cast<WriteFault>(fault);
        io->faults->writes = io->faults->commits = 0;
        auto result = d.save_as(target);
        QCOMPARE(rejected(result).code, static_cast<Code>(code));
        QCOMPARE(rejected(result).path, target);
        QCOMPARE(rejected(result).operation, ProjectOperation::save);
        QCOMPARE(read_file(target), QByteArray("previous destination bytes"));
        QCOMPARE(d.path(), old_path);
        QVERIFY(d.dirty());
        QCOMPARE(d.revision(), revision);
        QCOMPARE(d.saved_revision(), saved_revision);
        QVERIFY(d.workspace().capture() == snapshot);
        QCOMPARE(QDir(tmp.path()).entryList(QDir::Files | QDir::Hidden), files);
        QVERIFY(!rejected(result).message.contains("internal exception"));
        if (static_cast<WriteFault>(fault) == WriteFault::short_then_fail)
            QVERIFY(io->faults->writes > 1);
    }
    void partialWritesCompleteSuccessfully() {
        QTemporaryDir tmp;
        auto io = std::make_shared<FaultIo>();
        io->faults->write = WriteFault::short_success;
        ProjectFileStore store(io);
        auto snapshot = project::default_project();
        auto saved = store.save(snapshot, tmp.filePath("short.openece"));
        accepted(saved);
        QVERIFY(io->faults->writes > 1);
        QCOMPARE(io->faults->commits, 1);
        auto loaded = store.load(tmp.filePath("short.openece"));
        QVERIFY(accepted(loaded).snapshot == snapshot);
    }
    void injectedReadFailures_data() {
        QTest::addColumn<int>("fault");
        QTest::addColumn<int>("code");
        for (auto [fault, code] : {std::pair{ReadFault::open, Code::read_failed},
                                   {ReadFault::read, Code::read_failed},
                                   {ReadFault::oversized, Code::file_too_large},
                                   {ReadFault::short_read, Code::read_failed},
                                   {ReadFault::size_change, Code::read_failed}})
            QTest::newRow(qPrintable(QString::number(static_cast<int>(fault))))
                << static_cast<int>(fault) << static_cast<int>(code);
    }
    void injectedReadFailures() {
        QFETCH(int, fault);
        QFETCH(int, code);
        QTemporaryDir tmp;
        const auto path = tmp.filePath("input.openece");
        auto io = std::make_shared<FaultIo>();
        ProjectDocument d(std::make_unique<ProjectWorkspace>(),
                          std::make_shared<ProjectFileStore>(io));
        auto saved = d.save_as(path);
        accepted(saved);
        edit(d, "45");
        const auto snapshot = d.workspace().capture();
        const auto revision = d.revision();
        auto* view = &d.workspace();
        QSignalSpy states(&d, &ProjectDocument::stateChanged);
        io->faults->read = static_cast<ReadFault>(fault);
        auto result = d.prepare_open(path);
        QCOMPARE(rejected(result).code, static_cast<Code>(code));
        QVERIFY(d.workspace().capture() == snapshot);
        QCOMPARE(&d.workspace(), view);
        QCOMPARE(d.path(), path);
        QVERIFY(d.dirty());
        QCOMPARE(d.revision(), revision);
        QCOMPARE(states.count(), 0);
        if (static_cast<ReadFault>(fault) == ReadFault::oversized)
            QCOMPARE(io->faults->reads, 0);
    }
    void realOversizedAndGrowingInput() {
        QTemporaryDir tmp;
        const auto path = tmp.filePath("large.openece");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(static_cast<qint64>(project::limits::file_bytes + 1)));
        file.close();
        ProjectFileStore store;
        auto large = store.load(path);
        QCOMPARE(rejected(large).code, Code::file_too_large);
        auto io = std::make_shared<FaultIo>();
        io->faults->read = ReadFault::growth;
        auto growing = ProjectFileStore(io).load(path);
        QCOMPARE(rejected(growing).code, Code::file_too_large);
        QVERIFY(io->faults->reads > 1);
    }
    void decodeErrorsPreserveLiveSession_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<int>("code");
        QTest::newRow("syntax") << QByteArray("{") << int(Code::malformed_json);
        QTest::newRow("UTF8") << QByteArray("{\"x\":\"\xff\"}") << int(Code::invalid_utf8);
        QTest::newRow("duplicate") << QByteArray("{\"x\":0,\"x\":1}") << int(Code::duplicate_key);
        auto valid = QByteArray::fromStdString(project::encode_project(project::default_project()));
        auto version = valid;
        version.replace("\"schema_version\": 1", "\"schema_version\": 999");
        QTest::newRow("version") << version << int(Code::unsupported_version);
        auto format = valid;
        format.replace("org.openece.project", "some.other.format");
        QTest::newRow("format") << format << int(Code::wrong_format);
        auto type = valid;
        type.replace("\"schema_version\": 1", "\"schema_version\": true");
        QTest::newRow("type") << type << int(Code::wrong_type);
        auto structure = valid;
        structure.replace("\"signals\":", "\"not-a-domain\":");
        QTest::newRow("structure") << structure << int(Code::missing_field);
    }
    void decodeErrorsPreserveLiveSession() {
        QFETCH(QByteArray, bytes);
        QFETCH(int, code);
        QTemporaryDir tmp;
        const auto path = tmp.filePath("bad.openece");
        write_file(path, bytes);
        ProjectDocument d;
        edit(d, "90");
        d.workspace().findChild<QPushButton*>("generate")->click();
        const auto snapshot = d.workspace().capture();
        auto* view = &d.workspace();
        const auto revision = d.revision();
        auto result = d.prepare_open(path);
        QCOMPARE(rejected(result).code, static_cast<Code>(code));
        QCOMPARE(rejected(result).operation, ProjectOperation::decode);
        try {
            project::decode_project(
                std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
            QFAIL("Fixture must fail codec validation");
        } catch (const project::Error& e) {
            QCOMPARE(rejected(result).field_path, e.path());
            QVERIFY(rejected(result).byte_offset == e.offset());
        }
        QCOMPARE(&d.workspace(), view);
        QVERIFY(d.workspace().capture() == snapshot);
        QVERIFY(d.path().isEmpty());
        QVERIFY(d.dirty());
        QCOMPARE(d.revision(), revision);
    }
    void prepareFailuresAndFidelityMismatch() {
        QTemporaryDir tmp;
        const auto path = tmp.filePath("prepare.openece");
        auto saved = ProjectFileStore{}.save(project::default_project(), path);
        accepted(saved);
        for (bool throw_error : {true, false}) {
            QPointer<ProjectWorkspace> discarded;
            auto factory =
                [&](project::ProjectSnapshot snapshot) -> std::unique_ptr<ProjectWorkspace> {
                if (throw_error)
                    throw std::runtime_error("private implementation detail");
                auto w = std::make_unique<ProjectWorkspace>(std::move(snapshot));
                discarded = w.get();
                w->findChild<QLineEdit*>("phase")->setText("changed during restore");
                return w;
            };
            ProjectDocument d(std::make_unique<ProjectWorkspace>(),
                              std::make_shared<ProjectFileStore>(), factory);
            edit(d, "12");
            const auto snapshot = d.workspace().capture();
            auto* old = &d.workspace();
            auto candidate = d.prepare_open(path);
            QCOMPARE(rejected(candidate).code, Code::restore_failed);
            QVERIFY(discarded.isNull());
            QCOMPARE(&d.workspace(), old);
            QVERIFY(d.workspace().capture() == snapshot);
            QVERIFY(d.dirty());
            QVERIFY(!rejected(candidate).message.contains("private implementation"));
        }
    }
    void revisionsNewInstallAndCandidateCancellation() {
        QTemporaryDir tmp;
        ProjectDocument d;
        QVERIFY(!d.dirty());
        QCOMPARE(d.revision(), std::uint64_t{0});
        edit(d, "25");
        QVERIFY(d.dirty());
        auto revision = d.revision();
        d.workspace().findChild<QPushButton*>("generate")->click();
        QCOMPARE(d.revision(), revision);
        auto saved = d.save_as(tmp.filePath("revision.openece"));
        accepted(saved);
        QVERIFY(!d.dirty());
        QCOMPARE(d.saved_revision(), revision);
        {
            auto candidate = d.prepare_new(project::ProjectSnapshot{}, DocumentState::dirty);
            accepted(candidate); // abandoned, not installed
        }
        QCOMPARE(d.revision(), revision);
        QVERIFY(!d.dirty());
        auto candidate = d.prepare_new(project::ProjectSnapshot{}, DocumentState::dirty);
        auto installed = d.install(std::move(accepted(candidate)));
        accepted(installed);
        QVERIFY(d.dirty());
        QVERIFY(d.path().isEmpty());
        inert(d.workspace());
        auto clean = d.prepare_new(project::default_project(), DocumentState::clean);
        auto fresh = d.install(std::move(accepted(clean)));
        accepted(fresh);
        QVERIFY(!d.dirty());
        QCOMPARE(d.revision(), std::uint64_t{0});
    }
    void failedSaveOfCleanDocumentAndCancelledSaveAs() {
        auto io = std::make_shared<FaultIo>();
        ProjectDocument d(std::make_unique<ProjectWorkspace>(),
                          std::make_shared<ProjectFileStore>(io));
        QTemporaryDir tmp;
        auto saved = d.save_as(tmp.filePath("clean.openece"));
        accepted(saved);
        io->faults->write = WriteFault::commit;
        const auto revision = d.revision();
        auto failed = d.save();
        rejected(failed);
        QVERIFY(!d.dirty());
        QCOMPARE(d.revision(), revision);
        const auto writes = io->faults->writes;
        auto cancelled = d.save_as(std::nullopt);
        QCOMPARE(accepted(cancelled), SaveStatus::cancelled);
        QCOMPARE(io->faults->writes, writes);
        ProjectDocument unnamed;
        auto missing = unnamed.save();
        QCOMPARE(rejected(missing).code, Code::save_open_failed);
        QVERIFY(!unnamed.dirty());
    }
    void encodeFailureDoesNotOpenDestination() {
        QTemporaryDir tmp;
        auto io = std::make_shared<FaultIo>();
        ProjectDocument d(std::make_unique<ProjectWorkspace>(),
                          std::make_shared<ProjectFileStore>(io));
        const auto path = tmp.filePath("old.openece");
        auto saved = d.save_as(path);
        accepted(saved);
        const auto previous = read_file(path);
        const auto opens = io->faults->opens;
        // Name fields accept UTF-16 editor text; storage has an additional UTF-8 budget.
        auto* nodes = d.workspace().findChild<QTableWidget*>("circuit_nodes");
        nodes->item(0, 1)->setText(QString(4096, QChar(0x03c0)));
        auto failed = d.save();
        QCOMPARE(rejected(failed).code, Code::encode_failed);
        QVERIFY(!rejected(failed).field_path.empty());
        QVERIFY(d.dirty());
        QCOMPARE(io->faults->opens, opens);
        QCOMPARE(read_file(path), previous);
        QCOMPARE(d.path(), path);
    }
    void pendingEditorTextSavedExactlyWithoutExecution() {
        auto snapshot = project::default_project();
        snapshot.selected_domain = "digital";
        snapshot.digital.selected_tab = "timing";
        ProjectDocument d(std::make_unique<ProjectWorkspace>(snapshot));
        d.workspace().show();
        auto* table = d.workspace().findChild<QTableWidget*>("timing_inputs");
        table->editItem(table->item(0, 3));
        QCoreApplication::processEvents();
        QLineEdit* active = nullptr;
        for (auto* editor : table->findChildren<QLineEdit*>())
            if (editor->property("draftIndex").isValid())
                active = editor;
        QVERIFY(active);
        active->setText(" 1e-π ");
        QTemporaryDir tmp;
        const auto path = tmp.filePath("pending.openece");
        auto saved = d.save_as(path);
        accepted(saved);
        auto loaded = ProjectFileStore{}.load(path);
        QCOMPARE(accepted(loaded).snapshot.digital.timing.inputs[0].clock.first_edge_text,
                 std::string(" 1e-π "));
        QCOMPARE(active->text(), QString(" 1e-π "));
        QVERIFY(!d.dirty());
        inert(d.workspace());
    }
    void unknownFieldsRemainVisibleUntilResave() {
        QTemporaryDir tmp;
        const auto path = tmp.filePath("unknown.openece");
        auto bytes = QByteArray::fromStdString(project::encode_project(project::default_project()));
        bytes.insert(1, "\"future_note\":true,");
        write_file(path, bytes);
        ProjectDocument d;
        auto staged = d.prepare_open(path);
        QVERIFY(!accepted(staged).ignored_fields().empty());
        QVERIFY(d.ignored_fields().empty());
        auto installed = d.install(std::move(accepted(staged)));
        accepted(installed);
        QVERIFY(!d.ignored_fields().empty());
        auto saved = d.save();
        accepted(saved);
        QVERIFY(d.ignored_fields().empty());
        auto loaded = ProjectFileStore{}.load(path);
        QVERIFY(accepted(loaded).ignored_fields.empty());
    }
    void sameFileSaveAsRestagesPendingOpen() {
        QTemporaryDir tmp;
        const auto path = tmp.filePath("same.openece");
        ProjectFileStore store;
        auto written = store.save(project::default_project(), path);
        accepted(written);
        ProjectDocument d;
        auto staged = d.prepare_open(path);
        accepted(staged);
        edit(d, "new invalid π value");
        const auto current = d.workspace().capture();
        auto saved = d.save_as(tmp.path() + "/./same.openece");
        accepted(saved);
        auto installed = d.install(std::move(accepted(staged)));
        accepted(installed);
        QVERIFY(d.workspace().capture() == current);
        QVERIFY(!d.dirty());
        inert(d.workspace());
    }
    void restageFailurePreservesSessionAfterSave() {
        QTemporaryDir tmp;
        const auto path = tmp.filePath("same.openece");
        auto io = std::make_shared<FaultIo>();
        ProjectDocument d(std::make_unique<ProjectWorkspace>(),
                          std::make_shared<ProjectFileStore>(io));
        auto saved = d.save_as(path);
        accepted(saved);
        auto staged = d.prepare_open(path);
        accepted(staged);
        edit(d, "30");
        auto updated = d.save();
        accepted(updated);
        const auto snapshot = d.workspace().capture();
        auto* old = &d.workspace();
        io->faults->read = ReadFault::read;
        auto failed = d.install(std::move(accepted(staged)));
        QCOMPARE(rejected(failed).code, Code::read_failed);
        QCOMPARE(&d.workspace(), old);
        QVERIFY(d.workspace().capture() == snapshot);
        QCOMPARE(d.path(), path);
        QVERIFY(!d.dirty());
    }
    void pathIdentityAndFilesystemFailures() {
        QTemporaryDir tmp;
        QVERIFY(QDir(tmp.path()).mkpath("nested"));
        const auto path = tmp.filePath("identity.openece");
        write_file(path, "old");
        QVERIFY(equivalent_project_paths(path, tmp.path() + "/nested/../identity.openece"));
        QVERIFY(
            equivalent_project_paths(tmp.filePath("new.openece"), tmp.path() + "/./new.openece"));
        QVERIFY(!equivalent_project_paths({}, {}));
#ifdef Q_OS_WIN
        QVERIFY(equivalent_project_paths(path, path.toUpper()));
        QVERIFY(equivalent_project_paths(tmp.filePath("NEW.openece"), tmp.filePath("new.openece")));
#else
        QVERIFY(!equivalent_project_paths(path, tmp.filePath("IDENTITY.openece")));
        const auto alias = tmp.filePath("alias.openece");
        QVERIFY(QFile::link(path, alias));
        QVERIFY(equivalent_project_paths(path, alias));
        const auto dir_alias = tmp.filePath("alias-dir");
        QVERIFY(QFile::link(tmp.filePath("nested"), dir_alias));
        QVERIFY(equivalent_project_paths(dir_alias + "/new.openece",
                                         tmp.filePath("nested/new.openece")));
#endif
        ProjectFileStore store;
        auto missing = store.save(project::default_project(), tmp.filePath("absent/child.openece"));
        QCOMPARE(rejected(missing).code, Code::save_open_failed);
        auto directory = store.save(project::default_project(), tmp.filePath("nested"));
        QCOMPARE(rejected(directory).code, Code::save_open_failed);
        auto read_directory = store.load(tmp.path());
        QCOMPARE(rejected(read_directory).code, Code::read_failed);
        auto missing_read = store.load(tmp.filePath("missing"));
        QCOMPARE(rejected(missing_read).code, Code::read_failed);
        QCOMPARE(read_file(path), QByteArray("old"));
    }
    void readOnlyDirectoryWhenEnforced() {
#ifndef Q_OS_UNIX
        QSKIP("POSIX directory permissions are not portable to Windows ACLs; injected failures run "
              "everywhere.");
#else
        QTemporaryDir tmp;
        const auto path = tmp.filePath("readonly.openece");
        write_file(path, "old");
        const auto permissions = QFile::permissions(tmp.path());
        QVERIFY(QFile::setPermissions(tmp.path(), QFile::ReadOwner | QFile::ExeOwner));
        QFile probe(tmp.filePath("probe"));
        const bool can_write = probe.open(QIODevice::WriteOnly);
        probe.close();
        if (can_write) {
            QFile::setPermissions(tmp.path(), permissions);
            QSKIP("Effective privileges bypass read-only directory permissions.");
        }
        auto failed = ProjectFileStore{}.save(project::default_project(), path);
        QFile::setPermissions(tmp.path(), permissions);
        QCOMPARE(rejected(failed).code, Code::save_open_failed);
        QCOMPARE(read_file(path), QByteArray("old"));
#endif
    }
    void candidateCannotBeInstalledIntoAnotherDocument() {
        ProjectDocument a, b;
        auto staged = a.prepare_new(project::default_project(), DocumentState::clean);
        auto failed = b.install(std::move(accepted(staged)));
        QCOMPARE(rejected(failed).code, Code::restore_failed);
        QVERIFY(!a.dirty());
        QVERIFY(!b.dirty());
    }
};
QTEST_MAIN(ProjectDocumentTest)
#include "project_document_test.moc"
