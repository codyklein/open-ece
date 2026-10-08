#include "branding.hpp"
#include "main_window.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QTimer>
#include <QtEndian>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
namespace {
void require(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
#ifdef _WIN32
QByteArray resource(HMODULE module, WORD type, WORD id) {
    const auto found = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(type));
    require(found != nullptr, "Missing executable icon resource");
    const auto loaded = LoadResource(module, found);
    require(loaded != nullptr, "Cannot load executable icon resource");
    const auto* data = static_cast<const char*>(LockResource(loaded));
    require(data != nullptr, "Cannot read executable icon resource");
    return QByteArray(data, SizeofResource(module, found));
}
quint16 u16(const QByteArray& b, qsizetype offset) {
    require(offset >= 0 && offset + 2 <= b.size(), "Truncated icon header");
    return qFromLittleEndian<quint16>(b.constData() + offset);
}
quint32 u32(const QByteArray& b, qsizetype offset) {
    require(offset >= 0 && offset + 4 <= b.size(), "Truncated icon header");
    return qFromLittleEndian<quint32>(b.constData() + offset);
}
void executable_icon(const QDir& root) {
    QFile file(root.filePath("branding/openece.ico"));
    require(file.open(QIODevice::ReadOnly), "Missing packaged approved ICO");
    const auto ico = file.readAll();
    struct Module {
        HMODULE handle;
        ~Module() { FreeLibrary(handle); }
    } module{LoadLibraryExW(reinterpret_cast<LPCWSTR>(root.filePath("openece.exe").utf16()),
                            nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE)};
    require(module.handle != nullptr, "Cannot inspect packaged executable");
    const auto group = resource(module.handle, 14, 101); // RT_GROUP_ICON
    require(u16(group, 0) == 0 && u16(group, 2) == 1 && u16(group, 4) == 10 && u16(ico, 0) == 0 &&
                u16(ico, 2) == 1 && u16(ico, 4) == 10,
            "Expected ten executable icon resolutions");
    int index = 0;
    for (const int size : {16, 20, 24, 32, 40, 48, 64, 96, 128, 256}) {
        const qsizetype g = 6 + 14 * index, i = 6 + 16 * index;
        require(g + 14 <= group.size() && i + 16 <= ico.size(), "Truncated icon directory");
        const int width = static_cast<unsigned char>(group[g]);
        const int height = static_cast<unsigned char>(group[g + 1]);
        require((width ? width : 256) == size && (height ? height : 256) == size,
                "Incorrect executable icon dimensions");
        require(group.mid(g, 12) == ico.mid(i, 12), "Executable icon metadata differs");
        const auto length = u32(ico, i + 8), offset = u32(ico, i + 12);
        require(static_cast<quint64>(offset) + length <= static_cast<quint64>(ico.size()),
                "Truncated original icon frame");
        require(resource(module.handle, 3, u16(group, g + 12)) == ico.mid(offset, length),
                "Executable icon artwork differs from approved ICO");
        ++index;
    }
    std::cout << "PASS: executable icon: ten approved 16–256 px frames, byte-exact.\n";
}
#endif
} // namespace
void verify_packaged_branding() {
    using namespace openece::gui;
    require(QApplication::applicationVersion() == OPENECE_TEST_VERSION,
            "Incorrect application version");
    require(!QApplication::windowIcon().isNull(), "Missing application icon");
    MainWindow window;
    bool inspected = false;
    QTimer::singleShot(0, &window, [&] {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            inspected = box->text().contains(QString("OpenECE %1").arg(OPENECE_TEST_VERSION)) &&
                        box->text().contains("MIT") && !box->iconPixmap().isNull();
            box->accept();
        }
    });
    window.findChild<QAction*>("about_openece")->trigger();
    require(inspected, "Packaged About version/icon check failed");
#ifdef _WIN32
    const QDir root(QCoreApplication::applicationDirPath());
    // The build-tree test has no packaged examples/runtime folder.
    if (root.exists("examples"))
        executable_icon(root);
#endif
    std::cout << "PASS: packaged branding: application icon and About OpenECE "
              << OPENECE_TEST_VERSION << ".\n";
}
