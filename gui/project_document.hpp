#pragma once
#include "project_file_store.hpp"
#include "project_workspace.hpp"
#include <functional>
namespace openece::gui {
enum class DocumentState { clean, dirty };
enum class SaveStatus { saved, cancelled };
class ProjectDocument;
// Only a document can create an eligible candidate. Destroying one cancels preparation.
class PreparedProject {
  public:
    PreparedProject(PreparedProject&&) noexcept = default;
    PreparedProject& operator=(PreparedProject&&) noexcept = default;
    const project::ProjectSnapshot& snapshot() const { return workspace_->draft(); }
    const QString& path() const { return path_; }
    const std::vector<std::string>& ignored_fields() const { return ignored_; }

  private:
    friend class ProjectDocument;
    PreparedProject() = default;
    std::unique_ptr<ProjectWorkspace> workspace_;
    QString path_;
    std::vector<std::string> ignored_;
    std::shared_ptr<const int> owner_, save_token_;
    DocumentState state_ = DocumentState::clean;
    bool from_file_ = false;
};
// GUI-thread controller. No dialogs, filesystem preferences, or menu actions.
// It owns the workspace; any visual host must outlive it, and must not delete the workspace.
class ProjectDocument final : public QObject {
    Q_OBJECT
  public:
    using PrepareWorkspace =
        std::function<std::unique_ptr<ProjectWorkspace>(project::ProjectSnapshot)>;
    explicit ProjectDocument(
        std::unique_ptr<ProjectWorkspace> workspace = std::make_unique<ProjectWorkspace>(),
        std::shared_ptr<ProjectFileStore> store = std::make_shared<ProjectFileStore>(),
        PrepareWorkspace prepare = {}, QObject* parent = nullptr);
    ProjectWorkspace& workspace() const { return *workspace_; }
    const QString& path() const { return path_; }
    bool dirty() const { return dirty_; }
    std::uint64_t revision() const { return revision_; }
    std::uint64_t saved_revision() const { return saved_revision_; }
    const std::vector<std::string>& ignored_fields() const { return ignored_; }
    ProjectResult<PreparedProject> prepare_open(const QString&) const;
    ProjectResult<PreparedProject> prepare_new(project::ProjectSnapshot, DocumentState) const;
    ProjectStatus install(PreparedProject);
    ProjectResult<SaveStatus> save();
    // nullopt models cancelled destination selection: no capture or I/O occurs.
    ProjectResult<SaveStatus> save_as(const std::optional<QString>& destination);
  Q_SIGNALS:
    void stateChanged();
    // Old workspace stays alive throughout this synchronous notification.
    void workspaceReplaced(ProjectWorkspace* oldWorkspace, ProjectWorkspace* newWorkspace);

  private:
    ProjectResult<PreparedProject> prepare(project::ProjectSnapshot, QString,
                                           std::vector<std::string>, DocumentState, bool) const;
    ProjectResult<SaveStatus> save_to(const QString&);
    void bind(ProjectWorkspace*);
    std::unique_ptr<ProjectWorkspace> workspace_;
    std::shared_ptr<ProjectFileStore> store_;
    PrepareWorkspace prepare_;
    QString path_;
    std::vector<std::string> ignored_;
    std::shared_ptr<const int> owner_ = std::make_shared<const int>(0);
    std::shared_ptr<const int> save_token_ = std::make_shared<const int>(0);
    std::uint64_t revision_ = 0, saved_revision_ = 0;
    bool dirty_ = false;
};
} // namespace openece::gui
