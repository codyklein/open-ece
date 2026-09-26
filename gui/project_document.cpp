#include "project_document.hpp"
#include <limits>
namespace openece::gui {
namespace {
ProjectFailure preparation_failure(const QString& path) {
    return {project::ErrorCode::restore_failed,
            ProjectOperation::prepare,
            path,
            "Could not prepare an exact, inactive project workspace. The current project was kept.",
            {},
            {},
            {},
            {}};
}
} // namespace
ProjectDocument::ProjectDocument(std::unique_ptr<ProjectWorkspace> workspace,
                                 std::shared_ptr<ProjectFileStore> store, PrepareWorkspace prepare,
                                 QObject* parent)
    : QObject(parent), workspace_(std::move(workspace)), store_(std::move(store)),
      prepare_(std::move(prepare)) {
    if (!workspace_ || !store_ || workspace_->parent())
        throw std::invalid_argument("Document requires an owned, parentless workspace and store");
    if (!prepare_)
        prepare_ = [](project::ProjectSnapshot p) {
            return std::make_unique<ProjectWorkspace>(std::move(p), true);
        };
    bind(workspace_.get());
}
void ProjectDocument::bind(ProjectWorkspace* workspace) {
    connect(workspace, &DraftView::draftEdited, this, [this] {
        // Never wrap a revision into an apparently clean state.
        if (revision_ != std::numeric_limits<std::uint64_t>::max())
            ++revision_;
        dirty_ = true;
        Q_EMIT stateChanged();
    });
}
ProjectResult<PreparedProject> ProjectDocument::prepare(project::ProjectSnapshot snapshot,
                                                        QString path,
                                                        std::vector<std::string> ignored,
                                                        DocumentState state, bool from_file) const {
    try {
        project::validate_structure(snapshot);
    } catch (const project::Error& e) {
        return ProjectFailure{e.code(),   ProjectOperation::prepare,
                              path,       QString::fromUtf8(e.what()),
                              {},         e.path(),
                              e.offset(), e.code()};
    } catch (...) {
        return preparation_failure(path);
    }
    try {
        PreparedProject result;
        result.workspace_ = prepare_(snapshot);
        if (!result.workspace_ || result.workspace_->parent() ||
            result.workspace_->capture() != snapshot)
            return preparation_failure(path);
        result.path_ = std::move(path);
        result.ignored_ = std::move(ignored);
        result.state_ = state;
        result.from_file_ = from_file;
        result.owner_ = owner_;
        result.save_token_ = save_token_;
        return result;
    } catch (...) {
        return preparation_failure(path);
    }
}
ProjectResult<PreparedProject> ProjectDocument::prepare_open(const QString& path) const {
    auto loaded = store_->load(path);
    if (auto* error = std::get_if<ProjectFailure>(&loaded))
        return *error;
    auto& decoded = std::get<project::DecodedProject>(loaded);
    return prepare(std::move(decoded.snapshot), absolute_project_path(path),
                   std::move(decoded.ignored_fields), DocumentState::clean, true);
}
ProjectResult<PreparedProject> ProjectDocument::prepare_new(project::ProjectSnapshot snapshot,
                                                            DocumentState state) const {
    return prepare(std::move(snapshot), {}, {}, state, false);
}
ProjectStatus ProjectDocument::install(PreparedProject candidate) {
    if (candidate.owner_ != owner_ || !candidate.workspace_)
        return ProjectFailure{project::ErrorCode::restore_failed,
                              ProjectOperation::install,
                              candidate.path_,
                              "This candidate does not belong to the current document.",
                              {},
                              {},
                              {},
                              {}};
    // Any intervening successful save can have replaced the staged source through
    // aliases. Re-stage conservatively, including Save As to a pending Open path.
    if (candidate.from_file_ && candidate.save_token_ != save_token_) {
        auto refreshed = prepare_open(candidate.path_);
        if (auto* error = std::get_if<ProjectFailure>(&refreshed))
            return *error;
        candidate = std::move(std::get<PreparedProject>(refreshed));
    }
    try {
        bind(candidate.workspace_.get());
    } catch (...) {
        return preparation_failure(candidate.path_);
    }
    workspace_.swap(candidate.workspace_);
    path_.swap(candidate.path_);
    ignored_.swap(candidate.ignored_);
    revision_ = candidate.state_ == DocumentState::dirty ? 1 : 0;
    saved_revision_ = 0;
    dirty_ = candidate.state_ == DocumentState::dirty;
    disconnect(candidate.workspace_.get(), nullptr, this, nullptr);
    Q_EMIT workspaceReplaced(candidate.workspace_.get(), workspace_.get());
    Q_EMIT stateChanged();
    return std::monostate{};
}
ProjectResult<SaveStatus> ProjectDocument::save() { return save_to(path_); }
ProjectResult<SaveStatus> ProjectDocument::save_as(const std::optional<QString>& destination) {
    if (!destination)
        return SaveStatus::cancelled;
    return save_to(*destination);
}
ProjectResult<SaveStatus> ProjectDocument::save_to(const QString& destination) {
    QString path = absolute_project_path(destination);
    std::shared_ptr<const int> saved_token;
    std::uint64_t captured_revision = 0;
    try {
        const auto snapshot = workspace_->capture();
        captured_revision = revision_;
        // Allocate bookkeeping before commit. Signals below are post-commit
        // notifications, never part of the fallible file transaction.
        saved_token = std::make_shared<const int>(0);
        auto saved = store_->save(snapshot, path);
        if (auto* error = std::get_if<ProjectFailure>(&saved))
            return *error;
    } catch (...) {
        return ProjectFailure{project::ErrorCode::encode_failed,
                              ProjectOperation::encode,
                              path,
                              "Could not capture and encode the project. The destination and "
                              "document path were kept.",
                              {},
                              {},
                              {},
                              {}};
    }
    path_.swap(path);
    save_token_.swap(saved_token);
    saved_revision_ = captured_revision;
    dirty_ = revision_ != captured_revision;
    ignored_.clear();
    Q_EMIT stateChanged();
    return SaveStatus::saved;
}
} // namespace openece::gui
