// Release-only CI also runs this test beside the extracted runtime. It is not
// shipped.
#include "branding.hpp"
#include "main_window.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QListWidget>
#include <QPushButton>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
using namespace openece;
using namespace openece::gui;
void verify_packaged_branding();
namespace {
void require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
template <class T> T *control(QObject &root, const char *name) {
  auto *result = root.findChild<T *>(name);
  require(result != nullptr, name);
  return result;
}
class Dialogs final : public ProjectDialogs {
public:
  QString destination;
  QString failure;
  explicit Dialogs(QString path) : destination(std::move(path)) {}
  std::optional<QString> choose_open(const QString &) override {
    return destination + ".openece";
  }
  std::optional<QString> choose_save(const QString &) override {
    return destination;
  }
  bool overwrite(const QString &) override {
    failure = "Unexpected Save As overwrite";
    return false;
  }
  UnsavedChoice unsaved(const QString &) override {
    failure = "Unexpected dirty prompt";
    return UnsavedChoice::cancel;
  }
  void error(const ProjectFailure &e) override {
    failure = e.message + " " + e.detail;
  }
  void information(const QString &, const QString &) override {
    failure = "Unexpected project information dialog";
  }
};
QByteArray bytes(const QString &path) {
  QFile f(path);
  require(f.open(QIODevice::ReadOnly), "Cannot read saved file");
  return f.readAll();
}
void inert(ProjectWorkspace &w) {
  require(w.findChild<QWidget *>("transient_view")
                  ->findChild<QTableView *>("transient_trace")
                  ->model()
                  ->rowCount() == 0,
          "Restore populated transient trace");
  for (auto *timer : w.findChildren<QTimer *>())
    require(!timer->isActive(), "Restore started execution");
  for (auto name :
       {"digital_truth_table", "timing_results", "circuit_voltages",
        "ac_voltages", "ac_sweep_results", "comm_bits", "comm_ber_results"})
    require(control<QTableWidget>(w, name)->rowCount() == 0,
            "Restore populated derived table");
  for (auto *plot : w.findChildren<QwtPlot *>())
    for (auto *item : plot->itemList(QwtPlotItem::Rtti_PlotCurve))
      require(static_cast<QwtPlotCurve *>(item)->dataSize() == 0,
              "Restore populated derived plot");
}
void packaged_examples() {
  const QDir examples(QCoreApplication::applicationDirPath() + "/examples");
  // In a build-tree CTest run the shipped files are covered by
  // example_gui_test. In the fresh package job the script requires this
  // directory and all nine files.
  if (!examples.exists())
    return;
  const auto files = examples.entryList({"*.openece"}, QDir::Files);
  require(files.size() == 9, "Unexpected shipped example count");
  for (const auto &file : files) {
    const auto expected =
        project::decode_project(bytes(examples.filePath(file)).toStdString())
            .snapshot;
    ProjectDocument doc;
    auto candidate = doc.prepare_open(examples.filePath(file));
    require(std::holds_alternative<PreparedProject>(candidate),
            "Packaged example did not stage");
    require(std::holds_alternative<std::monostate>(
                doc.install(std::get<PreparedProject>(std::move(candidate)))),
            "Packaged example did not install");
    require(doc.workspace().capture() == expected,
            "Packaged example lost editable state");
    inert(doc.workspace());
    require(project::decode_project(
                project::encode_project(doc.workspace().capture()))
                    .snapshot == expected,
            "Packaged example round trip failed");
  }
  std::cout << "PASS: all nine packaged examples restored inertly and "
               "round-tripped.\n";
}
void run() {
  verify_packaged_branding();
  packaged_examples();
  QTemporaryDir temp(QDir::currentPath() + "/OpenECE persistence π 名 XXXXXX");
  require(temp.isValid(), "Cannot create Unicode test workspace");
  const QString base = temp.filePath("editable π project"),
                path = base + ".openece";
  auto dialogs = std::make_shared<Dialogs>(base);
  auto preferences = [&] {
    return std::make_shared<SettingsProjectPreferences>(
        std::make_unique<QSettings>(temp.filePath("preferences.ini"),
                                    QSettings::IniFormat));
  };
  project::ProjectSnapshot expected;
  auto initial = project::default_project();
  auto &transient = initial.circuits.transient;
  transient.nodes = {{{0}, "π ground"}};
  transient.next_node = {1};
  transient.next_component = {8};
  transient.components = {{{7},
                           "incomplete source",
                           "voltage_source",
                           project::Id{55},
                           std::nullopt,
                           {" 1e- ", "mV"},
                           {"constant", {{{"", "ms"}, " odd π "}}}}};
  transient.initial_conditions = {
      {project::Id{99}, "inductor_current", {"", "mA"}}};
  transient.probes = {
      {"current", "current", project::Id{100}, std::nullopt, project::Id{90}},
      {"voltage", "voltage", project::Id{80}, project::Id{81}, std::nullopt}};
  transient.stop = {" 1e- ", "s"};
  {
    MainWindow w(std::make_unique<ProjectDocument>(
                     std::make_unique<ProjectWorkspace>(initial, true)),
                 dialogs, preferences());
    w.show();
    QApplication::processEvents();
    control<QPushButton>(w, "generate")->click();
    control<QLineEdit>(w, "phase")->setText(" 1e-π ");
    auto *comb = control<QTableWidget>(w, "digital_inputs");
    comb->item(0, 1)->setText(" Input π ");
    control<QTableWidget>(w, "timing_elements")
        ->item(0, 2)
        ->setText(" 1, 999, ");
    auto *value = qobject_cast<QLineEdit *>(
        control<QTableWidget>(w, "circuit_components")->cellWidget(0, 5));
    require(value != nullptr, "Missing DC value editor");
    value->setText("");
    auto *nodes = control<QTableWidget>(w, "circuit_nodes");
    nodes->selectRow(2);
    control<QPushButton>(w, "circuit_remove_node")
        ->click(); // Retain dangling terminal IDs.
    auto *ac = control<QTableWidget>(w, "ac_components");
    ac->item(1, 1)->setText("R π 名");
    control<QComboBox>(w, "ac_frequency_unit")->setCurrentIndex(1);
    control<QLineEdit>(w, "ac_frequency")->setText("1e-");
    control<QComboBox>(w, "comm_source")->setCurrentIndex(1);
    control<QLineEdit>(w, "comm_manual")
        ->setText("101"); // Odd QPSK draft remains savable.
    control<QLineEdit>(w, "comm_bit_seed")->setText(" invalid seed ");
    control<QListWidget>(w, "domain_navigation")->setCurrentRow(3);
    control<QTabWidget>(w, "comm_tabs")->setCurrentIndex(2);
    require(w.document().dirty(), "User edits did not mark dirty");
    expected = w.document().workspace().capture();
    require(expected.circuits.dc.nodes.size() == 2 &&
                expected.circuits.dc.components[1].negative ==
                    project::Reference{project::Id{2}},
            "Dangling reference was not retained");
    control<QAction>(w, "project_save_as")->trigger();
    require(w.document().path() == path && !w.document().dirty(),
            "Save As failed");
    require(w.close(), "Saved window did not close");
  }
  const auto original = bytes(path);
  {
    MainWindow w(std::make_unique<ProjectDocument>(), dialogs, preferences());
    require(w.workflow().recent_projects() == QStringList{path},
            "Recent preference did not persist");
    require(w.document().path().isEmpty(),
            "Startup reopened a project automatically");
    w.show();
    QApplication::processEvents();
    control<QAction>(w, "project_open")->trigger();
    QApplication::processEvents();
    require(w.document().workspace().capture() == expected,
            "Cross-domain draft changed on load");
    require(!w.document().dirty(), "Restoration dirtied the document");
    inert(w.document().workspace());
    control<QLineEdit>(w, "phase")->setText(" -π / 3 ");
    expected.signals.phase.text = " -π / 3 ";
    control<QAction>(w, "project_save")->trigger();
    require(!w.document().dirty(), "Named Save failed");
    const auto replacement = bytes(path);
    require(replacement != original, "Named Save did not replace old bytes");
    auto decoded = project::decode_project(replacement.toStdString());
    require(decoded.snapshot == expected,
            "Atomic overwrite lost editable state");
    w.workflow().remove_recent(path);
    require(w.workflow().recent_projects().empty() &&
                bytes(path) == replacement,
            "Recent removal modified the project file");
    control<QAction>(w, "project_open")->trigger();
    require(w.document().workspace().capture() == expected,
            "Second load lost modifications");
    require(w.workflow().recent_projects() == QStringList{path},
            "Successful Open did not restore recent entry");
    inert(w.document().workspace());
    require(w.close(), "Reopened window did not exit normally");
  }
  std::cout << "PASS: packaged-runtime persistence: Unicode Save As, invalid "
               "cross-domain drafts, "
               "inert reopen, atomic overwrite, isolated recent preferences, "
               "normal close.\n";
}
} // namespace
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  configure_application_branding();
  app.setQuitOnLastWindowClosed(false);
  int result = 1;
  QTimer::singleShot(0, &app, [&] {
    try {
      run();
      result = 0;
    } catch (const std::exception &e) {
      std::cerr << "FAIL: " << e.what() << '\n';
    }
    app.exit(result);
  });
  app.exec();
  return result;
}
