#include "project_workspace.hpp"
#include "circuits_workspace.hpp"
#include "communications_view.hpp"
#include "digital_workspace.hpp"
#include "signals_dsp_view.hpp"
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
namespace openece::gui {
ProjectWorkspace::ProjectWorkspace(project::ProjectSnapshot snapshot, bool inert, QWidget* parent)
    : DraftView(parent), snapshot_(std::move(snapshot)) {
    project::validate_structure(snapshot_);
    const auto expected = snapshot_;
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    host_ = new QWidget(this);
    outer->addWidget(host_);
    try {
        auto* layout = new QHBoxLayout(host_);
        auto* navigation = new QListWidget(host_);
        navigation->setObjectName("domain_navigation");
        navigation->setAccessibleName("ECE domain");
        navigation->addItems({"Signals / DSP", "Digital Logic", "Circuits", "Communications"});
        navigation->setFixedWidth(145);
        auto* pages = new QStackedWidget(host_);
        pages->setObjectName("domain_pages");
        pages->addWidget(new SignalsDspView(pages, &snapshot_.signals, inert));
        pages->addWidget(new DigitalWorkspace(pages, &snapshot_.digital, inert));
        pages->addWidget(new CircuitsWorkspace(pages, &snapshot_.circuits, inert));
        pages->addWidget(new CommunicationsView(pages, &snapshot_.communications, inert));
        layout->addWidget(navigation);
        layout->addWidget(pages, 1);
        const QStringList tokens{"signals", "digital", "circuits", "communications"};
        connect(navigation, &QListWidget::currentRowChanged, pages,
                &QStackedWidget::setCurrentIndex);
        navigation->setCurrentRow(
            static_cast<int>(tokens.indexOf(qt_text(snapshot_.selected_domain))));
        connect(navigation, &QListWidget::currentRowChanged, this, [this, tokens](int row) {
            if (row >= 0)
                edit(snapshot_.selected_domain, draft_text(tokens[row]));
        });
        for (int i = 0; i < pages->count(); ++i)
            connect(static_cast<DraftView*>(pages->widget(i)), &DraftView::draftEdited, this,
                    &DraftView::draftEdited);
        restoring_ = false;
        if (inert && capture() != expected)
            throw project::Error(project::ErrorCode::invalid_value, "",
                                 "Workspace restoration changed the project draft");
    } catch (...) {
        delete host_;
        host_ = nullptr;
        throw;
    }
}
ProjectWorkspace::~ProjectWorkspace() { delete host_; }
void ProjectWorkspace::synchronize_pending_text() {
    // Direct domain roots synchronize their nested editors. Capture reads only snapshot_.
    auto* pages = host_->findChild<QStackedWidget*>("domain_pages");
    for (int i = 0; i < pages->count(); ++i)
        static_cast<DraftView*>(pages->widget(i))->synchronize_pending_text();
}
project::ProjectSnapshot ProjectWorkspace::capture() {
    synchronize_pending_text();
    return snapshot_;
}
} // namespace openece::gui
