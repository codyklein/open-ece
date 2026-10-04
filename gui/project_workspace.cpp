#include "project_workspace.hpp"
#include "circuits_workspace.hpp"
#include "communications_view.hpp"
#include "digital_workspace.hpp"
#include "signals_dsp_view.hpp"
#include <QApplication>
#include <QFocusFrame>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPainter>
#include <QPointer>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
namespace openece::gui {
namespace {
// Native selection highlights can outlive focus. One contrasting outline tracks
// actual focus; Qt's QFocusFrame owns positioning/scrolling and never takes input.
class CurrentFocusFrame final : public QFocusFrame {
  public:
    explicit CurrentFocusFrame(QWidget* parent) : QFocusFrame(parent) {
        setObjectName("keyboard_focus_indicator");
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setPen(Qt::black);
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
        painter.setPen(Qt::white);
        painter.drawRect(rect().adjusted(1, 1, -2, -2));
    }
};
} // namespace

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
        navigation->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        navigation->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        auto* pages = new QStackedWidget(host_);
        pages->setObjectName("domain_pages");
        pages->addWidget(new SignalsDspView(pages, &snapshot_.signals, inert));
        pages->addWidget(new DigitalWorkspace(pages, &snapshot_.digital, inert));
        pages->addWidget(new CircuitsWorkspace(pages, &snapshot_.circuits, inert));
        pages->addWidget(new CommunicationsView(pages, &snapshot_.communications, inert));
        layout->addWidget(navigation);
        auto* scroll = new QScrollArea(host_);
        scroll->setObjectName("workspace_scroll");
        scroll->setAccessibleName("Scrollable engineering workspace");
        scroll->setWidgetResizable(true);
        scroll->setWidget(pages);
        layout->addWidget(scroll, 1);
        const QStringList tokens{"signals", "digital", "circuits", "communications"};
        connect(navigation, &QListWidget::currentRowChanged, pages,
                &QStackedWidget::setCurrentIndex);
        navigation->setCurrentRow(
            static_cast<int>(tokens.indexOf(qt_text(snapshot_.selected_domain))));
        connect(navigation, &QListWidget::currentRowChanged, this, [this, tokens](int row) {
            if (!restoring_ && row >= 0 && row < tokens.size())
                snapshot_.selected_domain =
                    draft_text(tokens[row]); // Saved choice, not a data edit.
        });
        for (int i = 0; i < pages->count(); ++i)
            connect(static_cast<DraftView*>(pages->widget(i)), &DraftView::draftEdited, this,
                    &DraftView::draftEdited);
        QPointer<QFocusFrame> focus = new CurrentFocusFrame(host_);
        connect(qApp, &QApplication::focusChanged, this, [this, focus](QWidget*, QWidget* now) {
            if (focus)
                focus->setWidget(now && isAncestorOf(now) ? now : nullptr);
        });
        navigation->setFocus(Qt::OtherFocusReason);
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
void ProjectWorkspace::stop_execution() {
    for (auto* timer : findChildren<QTimer*>())
        timer->stop();
}
project::ProjectSnapshot ProjectWorkspace::capture() {
    synchronize_pending_text();
    return snapshot_;
}
} // namespace openece::gui
