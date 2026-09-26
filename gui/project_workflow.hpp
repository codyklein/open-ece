#pragma once
#include "project_document.hpp"
#include <QSettings>
#include <QStringList>
namespace openece::gui {
enum class UnsavedChoice { save, discard, cancel };
// Dialogs make decisions only; document mutations remain in ProjectWorkflow/ProjectDocument.
class ProjectDialogs {
  public:
    virtual ~ProjectDialogs() = default;
    virtual std::optional<QString> choose_open(const QString& current) = 0;
    virtual std::optional<QString> choose_save(const QString& current) = 0;
    virtual bool overwrite(const QString& destination) = 0;
    virtual UnsavedChoice unsaved(const QString& name) = 0;
    virtual void error(const ProjectFailure&) = 0;
    virtual void information(const QString& title, const QString& message) = 0;
};
std::shared_ptr<ProjectDialogs> qt_project_dialogs(QWidget* parent);
class ProjectPreferences {
  public:
    virtual ~ProjectPreferences() = default;
    virtual QStringList read_recent() = 0;
    // Preference failures never change a successful document transaction into a failure.
    virtual bool write_recent(const QStringList&) = 0;
};
class SettingsProjectPreferences final : public ProjectPreferences {
  public:
    explicit SettingsProjectPreferences(
        std::unique_ptr<QSettings> settings = std::make_unique<QSettings>("OpenECE", "OpenECE"));
    QStringList read_recent() override;
    bool write_recent(const QStringList&) override;

  private:
    std::unique_ptr<QSettings> settings_;
};
QString project_save_destination(QString);
QString project_window_title(const ProjectDocument&);
// GUI-thread workflow controller. Modal-dialog reentry is rejected. It borrows the document;
// the host and document must outlive it. No workflow state is serialized in project files.
class ProjectWorkflow final : public QObject {
    Q_OBJECT
  public:
    ProjectWorkflow(ProjectDocument&, std::shared_ptr<ProjectDialogs>,
                    std::shared_ptr<ProjectPreferences>, QObject* parent = nullptr);
    bool new_project();
    bool open_project();
    bool open_path(const QString&);
    bool save();
    bool save_as();
    bool request_close();
    const QStringList& recent_projects() const { return recent_; }
    void remove_recent(const QString&);
  Q_SIGNALS:
    void recentChanged();

  private:
    bool replace(ProjectResult<PreparedProject>);
    bool resolve_unsaved();
    bool save_impl(bool save_as);
    bool synchronize();
    void remember(const QString&);
    void write_preferences();
    ProjectDocument& document_;
    std::shared_ptr<ProjectDialogs> dialogs_;
    std::shared_ptr<ProjectPreferences> preferences_;
    QStringList recent_;
    bool busy_ = false;
};
} // namespace openece::gui
