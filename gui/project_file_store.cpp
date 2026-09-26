#include "project_file_store.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <algorithm>
#include <array>
namespace openece::gui {
namespace {
using Code = project::ErrorCode;
ProjectFailure failure(Code code, ProjectOperation op, const QString& path, const QString& message,
                       const QString& detail = {}) {
    return {code, op, path, message, detail, {}, {}, {}};
}
ProjectFailure codec_failure(const project::Error& e, ProjectOperation op, const QString& path) {
    return {op == ProjectOperation::encode ? Code::encode_failed : e.code(),
            op,
            path,
            QString::fromUtf8(e.what()),
            {},
            e.path(),
            e.offset(),
            e.code()};
}
class QtReader final : public ProjectReader {
    QFile file_;
    QString reason_;

  public:
    explicit QtReader(const QString& path) : file_(path) {}
    bool open() override {
        if (!QFileInfo(file_).isFile()) {
            reason_ = "The path does not name an existing regular file.";
            return false;
        }
        return file_.open(QIODevice::ReadOnly) && !file_.isSequential();
    }
    qint64 size() const override { return file_.size(); }
    qint64 read(char* bytes, qint64 size) override { return file_.read(bytes, size); }
    bool failed() const override { return file_.error() != QFileDevice::NoError; }
    QString error() const override { return reason_.isEmpty() ? file_.errorString() : reason_; }
};
class QtWriter final : public ProjectWriter {
    QSaveFile file_;

  public:
    explicit QtWriter(const QString& path) : file_(path) { file_.setDirectWriteFallback(false); }
    bool open() override { return file_.open(QIODevice::WriteOnly); }
    qint64 write(const char* bytes, qint64 size) override { return file_.write(bytes, size); }
    bool failed() const override { return file_.error() != QFileDevice::NoError; }
    bool commit() override { return file_.commit(); }
    QString error() const override { return file_.errorString(); }
};
class QtIo final : public ProjectFileIo {
  public:
    std::unique_ptr<ProjectReader> reader(const QString& path) override {
        return std::make_unique<QtReader>(path);
    }
    std::unique_ptr<ProjectWriter> writer(const QString& path) override {
        return std::make_unique<QtWriter>(path);
    }
};
QString identity(const QString& path) {
    if (path.isEmpty() || path.contains(QChar{0}))
        return {};
    QFileInfo info(path);
    const auto canonical = info.canonicalFilePath();
    if (!canonical.isEmpty())
        return canonical;
    QStringList tail;
    auto current = info.absoluteFilePath();
    for (;;) {
        QFileInfo ancestor(current);
        const auto resolved = ancestor.canonicalFilePath();
        if (!resolved.isEmpty())
            return QDir::cleanPath(resolved + '/' + tail.join('/'));
        tail.prepend(ancestor.fileName());
        const auto parent = ancestor.absolutePath();
        if (parent == current)
            return QDir::cleanPath(info.absoluteFilePath());
        current = parent;
    }
}
} // namespace
std::shared_ptr<ProjectFileIo> qt_project_file_io() { return std::make_shared<QtIo>(); }
QString absolute_project_path(const QString& path) {
    return path.isEmpty() ? QString{} : QFileInfo(path).absoluteFilePath();
}
bool equivalent_project_paths(const QString& a, const QString& b) {
    const auto left = identity(a), right = identity(b);
    if (left.isEmpty() || right.isEmpty())
        return false;
#ifdef Q_OS_WIN
    // QFileInfo's existing-path equality follows the filesystem's case rules.
    if (QFileInfo(a).exists() && QFileInfo(b).exists())
        return QFileInfo(left) == QFileInfo(right);
    return left.compare(right, Qt::CaseInsensitive) == 0;
#else
    return left == right;
#endif
}
ProjectFileStore::ProjectFileStore(std::shared_ptr<ProjectFileIo> io) : io_(std::move(io)) {
    if (!io_)
        throw std::invalid_argument("ProjectFileStore requires a backend");
}
ProjectResult<project::DecodedProject> ProjectFileStore::load(const QString& supplied) const {
    const auto path = absolute_project_path(supplied);
    auto op = ProjectOperation::read;
    try {
        if (path.isEmpty() || path.contains(QChar{0}))
            return failure(Code::read_failed, op, path, "Choose a valid project file path.");
        auto file = io_->reader(path);
        if (!file || !file->open())
            return failure(Code::read_failed, op, path, "Could not open the project for reading.",
                           file ? file->error() : QString{});
        const auto expected = file->size();
        if (expected < 0 || file->failed())
            return failure(Code::read_failed, op, path, "Could not determine project file size.",
                           file->error());
        if (static_cast<std::uint64_t>(expected) > project::limits::file_bytes)
            return failure(Code::file_too_large, op, path, "Project file exceeds the 8 MiB limit.");
        std::string bytes;
        bytes.reserve(static_cast<std::size_t>(expected));
        std::array<char, 65536> buffer;
        for (;;) {
            const auto count = file->read(buffer.data(), static_cast<qint64>(buffer.size()));
            if (count < 0 || count > static_cast<qint64>(buffer.size()) || file->failed())
                return failure(Code::read_failed, op, path, "Could not read the complete project.",
                               file->error());
            if (count == 0)
                break;
            const auto size = static_cast<std::size_t>(count);
            if (size > project::limits::file_bytes - bytes.size())
                return failure(Code::file_too_large, op, path,
                               "Project file grew beyond the 8 MiB limit.");
            bytes.append(buffer.data(), size);
        }
        if (bytes.size() != static_cast<std::size_t>(expected) || file->size() != expected ||
            file->failed())
            return failure(Code::read_failed, op, path,
                           "Project file changed or was incompletely read. Try opening it again.",
                           file->error());
        op = ProjectOperation::decode;
        return project::decode_project(bytes);
    } catch (const project::Error& e) {
        return codec_failure(e, op, path);
    } catch (...) {
        return failure(
            Code::read_failed, op, path,
            "Could not read or decode the project. Check file access and available memory.");
    }
}
ProjectStatus ProjectFileStore::save(const project::ProjectSnapshot& snapshot,
                                     const QString& supplied) const {
    auto op = ProjectOperation::encode;
    auto code = Code::encode_failed;
    const auto path = absolute_project_path(supplied);
    try {
        // The codec validates storage limits and bounds the UTF-8 output.
        const auto bytes = project::encode_project(snapshot);
        op = ProjectOperation::save;
        code = Code::save_open_failed;
        if (path.isEmpty() || path.contains(QChar{0}))
            return failure(code, op, path, "Choose a destination with Save As.");
        auto file = io_->writer(path);
        if (!file || !file->open())
            return failure(code, op, path,
                           "Could not create an atomic save in the destination directory.",
                           file ? file->error() : QString{});
        code = Code::save_write_failed;
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto remaining = static_cast<qint64>(bytes.size() - offset);
            const auto count = file->write(bytes.data() + offset, remaining);
            if (count <= 0 || count > remaining || file->failed())
                return failure(
                    code, op, path,
                    "Could not write the complete project; the previous file was preserved.",
                    file->error());
            offset += static_cast<std::size_t>(count);
        }
        code = Code::save_commit_failed;
        if (!file->commit())
            return failure(code, op, path,
                           "Could not replace the destination; the previous file was preserved.",
                           file->error());
        return std::monostate{};
    } catch (const project::Error& e) {
        return codec_failure(e, op, path);
    } catch (...) {
        return failure(
            code, op, path,
            "Project save failed. Check destination access, free space and available memory.");
    }
}
} // namespace openece::gui
