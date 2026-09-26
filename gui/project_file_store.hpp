#pragma once
#include <QString>
#include <memory>
#include <openece/project/model.hpp>
#include <variant>
namespace openece::gui {
enum class ProjectOperation { read, decode, prepare, encode, save, install };
struct ProjectFailure {
    project::ErrorCode code;
    ProjectOperation operation;
    QString path, message, detail;
    std::string field_path;
    std::optional<std::size_t> byte_offset;
    std::optional<project::ErrorCode> cause;
};
template <class T> using ProjectResult = std::variant<T, ProjectFailure>;
using ProjectStatus = ProjectResult<std::monostate>;
// Narrow device seams. Production implementations use QFile and QSaveFile only.
// Writers must preserve the destination unless commit succeeds; destruction aborts.
class ProjectReader {
  public:
    virtual ~ProjectReader() = default;
    virtual bool open() = 0;
    virtual qint64 size() const = 0;
    virtual qint64 read(char*, qint64) = 0;
    virtual bool failed() const = 0;
    virtual QString error() const = 0;
};
class ProjectWriter {
  public:
    virtual ~ProjectWriter() = default;
    virtual bool open() = 0;
    virtual qint64 write(const char*, qint64) = 0;
    virtual bool failed() const = 0;
    virtual bool commit() = 0;
    virtual QString error() const = 0;
};
class ProjectFileIo {
  public:
    virtual ~ProjectFileIo() = default;
    virtual std::unique_ptr<ProjectReader> reader(const QString&) = 0;
    virtual std::unique_ptr<ProjectWriter> writer(const QString&) = 0;
};
std::shared_ptr<ProjectFileIo> qt_project_file_io();
QString absolute_project_path(const QString&);
// Canonical existing path, or canonical ancestor + remaining path for a new file.
// Identity is the replacement destination, not inode identity of separate hard links.
bool equivalent_project_paths(const QString&, const QString&);
class ProjectFileStore {
  public:
    explicit ProjectFileStore(std::shared_ptr<ProjectFileIo> io = qt_project_file_io());
    ProjectResult<project::DecodedProject> load(const QString&) const;
    ProjectStatus save(const project::ProjectSnapshot&, const QString&) const;

  private:
    std::shared_ptr<ProjectFileIo> io_;
};
} // namespace openece::gui
