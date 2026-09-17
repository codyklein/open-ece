#include "digital_workspace.hpp"
#include "digital_logic_view.hpp"
#include "timing_view.hpp"
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace openece::gui {
DigitalWorkspace::DigitalWorkspace(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName("digital_tabs");
    auto* combinational = new DigitalLogicView(tabs);
    auto* timing = new TimingView(tabs);
    tabs->addTab(combinational, "Combinational");
    tabs->addTab(timing, "Timing / Sequential");
    layout->addWidget(tabs);
    auto* row = new QHBoxLayout;
    auto* copy = new QPushButton("Copy validated combinational circuit to timing", this);
    copy->setObjectName("digital_copy_timing");
    auto* delay = new QSpinBox(this);
    delay->setObjectName("digital_copy_delay");
    delay->setRange(1, 1'000'000'000);
    delay->setValue(1000);
    delay->setSuffix(" ps");
    row->addWidget(copy);
    row->addWidget(new QLabel("Gate delay:", this));
    row->addWidget(delay);
    row->addStretch();
    layout->addLayout(row);
    auto* status = new QLabel(this);
    status->setObjectName("digital_copy_status");
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    layout->addWidget(status);
    connect(copy, &QPushButton::clicked, this, [=] {
        try {
            timing->load_combinational(combinational->validated_circuit(),
                                       combinational->input_values(),
                                       {static_cast<std::uint64_t>(delay->value())});
            tabs->setCurrentIndex(1);
            status->setText(
                "Copied independently. Timing edits do not change the combinational circuit.");
        } catch (const std::exception& error) {
            status->setText("Cannot copy draft: " + QString::fromUtf8(error.what()));
        }
    });
}
} // namespace openece::gui
