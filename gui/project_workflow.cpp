#include "project_workflow.hpp"
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QScopedValueRollback>
namespace openece::gui {
namespace {
class QtProjectDialogs final : public ProjectDialogs {
    QWidget* parent_;

  public:
    explicit QtProjectDialogs(QWidget* parent) : parent_(parent) {}
    std::optional<QString> choose_open(const QString& current) override {
        auto path = QFileDialog::getOpenFileName(parent_, "Open project", current,
                                                 "OpenECE projects (*.openece);;All files (*)");
        return path.isEmpty() ? std::nullopt : std::optional{path};
    }
    std::optional<QString> choose_save(const QString& current) override {
        // Confirm only after appending the extension, against the actual destination.
        auto path = QFileDialog::getSaveFileName(
            parent_, "Save project as", current.isEmpty() ? "Untitled.openece" : current,
            "OpenECE projects (*.openece)", nullptr, QFileDialog::DontConfirmOverwrite);
        return path.isEmpty() ? std::nullopt : std::optional{path};
    }
    bool overwrite(const QString& path) override {
        QMessageBox box(QMessageBox::Question, "Replace project file?",
                        "Replace the existing file?\n" + path,
                        QMessageBox::Save | QMessageBox::Cancel, parent_);
        box.setTextFormat(Qt::PlainText);
        box.setDefaultButton(QMessageBox::Cancel);
        return box.exec() == QMessageBox::Save;
    }
    UnsavedChoice unsaved(const QString& name) override {
        QMessageBox box(QMessageBox::Warning, "Unsaved project", "Save changes to " + name + "?",
                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, parent_);
        box.setTextFormat(Qt::PlainText);
        box.setDefaultButton(QMessageBox::Save);
        box.setEscapeButton(QMessageBox::Cancel);
        switch (box.exec()) {
        case QMessageBox::Save:
            return UnsavedChoice::save;
        case QMessageBox::Discard:
            return UnsavedChoice::discard;
        default:
            return UnsavedChoice::cancel;
        }
    }
    void error(const ProjectFailure& e) override {
        QString context = e.path;
        if (!e.field_path.empty())
            context += "\nField: " + qt_text(e.field_path);
        if (e.byte_offset)
            context += "\nByte offset: " + QString::number(*e.byte_offset);
        if (!e.detail.isEmpty())
            context += "\n" + e.detail;
        QMessageBox box(QMessageBox::Critical, "Project operation failed", e.message,
                        QMessageBox::Ok, parent_);
        box.setTextFormat(Qt::PlainText);
        box.setInformativeText(context);
        box.exec();
    }
    void information(const QString& title, const QString& message) override {
        QMessageBox box(QMessageBox::Information, title, message, QMessageBox::Ok, parent_);
        box.setTextFormat(Qt::PlainText);
        box.exec();
    }
};
QStringList bounded_recent(const QStringList& paths) {
    QStringList result;
    for (const auto& path : paths) {
        if (path.isEmpty() || path.contains(QChar::Null) || !QFileInfo(path).isAbsolute())
            continue;
        if (std::none_of(result.begin(), result.end(), [&](const QString& other) {
                return equivalent_project_paths(path, other);
            }))
            result.push_back(absolute_project_path(path));
        if (result.size() == 10)
            break;
    }
    return result;
}
} // namespace
std::shared_ptr<ProjectDialogs> qt_project_dialogs(QWidget* parent) {
    return std::make_shared<QtProjectDialogs>(parent);
}
SettingsProjectPreferences::SettingsProjectPreferences(std::unique_ptr<QSettings> settings)
    : settings_(std::move(settings)) {}
QStringList SettingsProjectPreferences::read_recent() {
    return settings_->value("projects/recentPaths").toStringList();
}
bool SettingsProjectPreferences::write_recent(const QStringList& paths) {
    settings_->setValue("projects/recentPaths", paths);
    settings_->sync();
    return settings_->status() == QSettings::NoError;
}
QString project_save_destination(QString path) {
    if (!path.endsWith(".openece", Qt::CaseInsensitive))
        path += ".openece";
    return path;
}
QString project_window_title(const ProjectDocument& d) {
    auto name = d.path().isEmpty() ? QString("Untitled") : QFileInfo(d.path()).fileName();
    return name + (d.dirty() ? "*" : "") + " — OpenECE";
}
ProjectWorkflow::ProjectWorkflow(ProjectDocument& document, std::shared_ptr<ProjectDialogs> dialogs,
                                 std::shared_ptr<ProjectPreferences> preferences, QObject* parent)
    : QObject(parent), document_(document), dialogs_(std::move(dialogs)),
      preferences_(std::move(preferences)) {
    recent_ = bounded_recent(preferences_->read_recent());
}
bool ProjectWorkflow::synchronize() {
    try {
        document_.workspace().synchronize_pending_text();
        return true;
    } catch (...) {
        dialogs_->error({project::ErrorCode::encode_failed,
                         ProjectOperation::encode,
                         document_.path(),
                         "Could not capture pending edits. The project was kept.",
                         {},
                         {},
                         {},
                         {}});
        return false;
    }
}
bool ProjectWorkflow::save_impl(bool save_as) {
    // Read the exact editor buffers before a modal dialog can move focus/commit delegates.
    if (!synchronize())
        return false;
    ProjectResult<SaveStatus> result = SaveStatus::cancelled;
    if (save_as || document_.path().isEmpty()) {
        auto selected = dialogs_->choose_save(document_.path());
        if (!selected || selected->isEmpty())
            return false;
        auto destination = project_save_destination(*selected);
        QFileInfo info(destination);
        if ((info.exists() || info.isSymLink()) && !dialogs_->overwrite(destination))
            return false;
        result = document_.save_as(destination);
    } else
        result = document_.save();
    if (auto* e = std::get_if<ProjectFailure>(&result)) {
        dialogs_->error(*e);
        return false;
    }
    if (std::get<SaveStatus>(result) != SaveStatus::saved)
        return false;
    remember(document_.path());
    return true;
}
bool ProjectWorkflow::resolve_unsaved() {
    if (!synchronize())
        return false;
    if (!document_.dirty())
        return true;
    switch (dialogs_->unsaved(
        document_.path().isEmpty() ? "Untitled" : QFileInfo(document_.path()).fileName())) {
    case UnsavedChoice::save:
        return save_impl(false);
    case UnsavedChoice::discard:
        return true;
    case UnsavedChoice::cancel:
        return false;
    }
    return false;
}
bool ProjectWorkflow::replace(ProjectResult<PreparedProject> staged) {
    if (auto* e = std::get_if<ProjectFailure>(&staged)) {
        dialogs_->error(*e);
        return false;
    }
    if (!resolve_unsaved())
        return false;
    auto installed = document_.install(std::move(std::get<PreparedProject>(staged)));
    if (auto* e = std::get_if<ProjectFailure>(&installed)) {
        dialogs_->error(*e);
        return false;
    }
    if (!document_.path().isEmpty())
        remember(document_.path());
    if (!document_.ignored_fields().empty())
        dialogs_->information("Unrecognized project fields",
                              "This project contains optional fields that OpenECE does not "
                              "interpret or retain. They will be discarded if you save it.");
    return true;
}
bool ProjectWorkflow::new_project() {
    if (busy_)
        return false;
    QScopedValueRollback guard(busy_, true);
    return replace(document_.prepare_new(project::default_project(), DocumentState::clean));
}
bool ProjectWorkflow::open_project() {
    if (busy_)
        return false;
    QScopedValueRollback guard(busy_, true);
    const auto path = dialogs_->choose_open(document_.path());
    if (!path || path->isEmpty())
        return false;
    return replace(document_.prepare_open(*path));
}
bool ProjectWorkflow::open_path(const QString& path) {
    if (busy_)
        return false;
    QScopedValueRollback guard(busy_, true);
    return replace(document_.prepare_open(path));
}
bool ProjectWorkflow::save() {
    if (busy_)
        return false;
    QScopedValueRollback guard(busy_, true);
    return save_impl(false);
}
bool ProjectWorkflow::save_as() {
    if (busy_)
        return false;
    QScopedValueRollback guard(busy_, true);
    return save_impl(true);
}
bool ProjectWorkflow::request_close() {
    if (busy_)
        return false;
    QScopedValueRollback guard(busy_, true);
    if (!resolve_unsaved())
        return false;
    document_.workspace().stop_execution();
    return true;
}
void ProjectWorkflow::write_preferences() {
    const bool saved = preferences_->write_recent(recent_);
    Q_EMIT recentChanged();
    if (!saved)
        dialogs_->information("Recent projects not saved",
                              "The project operation succeeded, but recent-project preferences "
                              "could not be written. Check your application settings permissions.");
}
void ProjectWorkflow::remember(const QString& path) {
    recent_.removeIf([&](const QString& other) { return equivalent_project_paths(path, other); });
    recent_.prepend(absolute_project_path(path));
    while (recent_.size() > 10)
        recent_.removeLast();
    write_preferences();
}
void ProjectWorkflow::remove_recent(const QString& path) {
    if (busy_)
        return;
    QScopedValueRollback guard(busy_, true);
    const auto before = recent_.size();
    recent_.removeIf([&](const QString& other) { return equivalent_project_paths(path, other); });
    if (before != recent_.size())
        write_preferences();
}
} // namespace openece::gui
