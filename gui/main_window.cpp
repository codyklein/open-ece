#include "main_window.hpp"
#include "circuits_workspace.hpp"
#include "digital_workspace.hpp"
#include "signals_dsp_view.hpp"

#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>

namespace openece::gui {
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("OpenECE — Signals / DSP, Digital Logic and Circuits");
    resize(1280, 820);
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* layout = new QHBoxLayout(central);
    auto* navigation = new QListWidget(central);
    navigation->setObjectName("domain_navigation");
    navigation->setAccessibleName("ECE domain");
    navigation->addItems({"Signals / DSP", "Digital Logic", "Circuits"});
    navigation->setFixedWidth(145);
    auto* pages = new QStackedWidget(central);
    pages->setObjectName("domain_pages");
    pages->addWidget(new SignalsDspView(pages));
    pages->addWidget(new DigitalWorkspace(pages));
    pages->addWidget(new CircuitsWorkspace(pages));
    layout->addWidget(navigation);
    layout->addWidget(pages, 1);
    connect(navigation, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    navigation->setCurrentRow(0);
}
} // namespace openece::gui
