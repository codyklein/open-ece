#include "circuit_presentation.hpp"
#include "draft_view.hpp"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <schematic/qt/canvas.hpp>
namespace openece::gui {
CircuitPresentation::CircuitPresentation(DraftView* owner, QWidget* tables, const QString& prefix,
                                         Project project, QWidget* parent)
    : QWidget(parent), owner_(owner), project_(std::move(project)) {
    setObjectName(prefix + "_presentation");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* row = new QHBoxLayout;
    auto* label = new QLabel("Circuit &view:", this);
    choice_ = new QComboBox(this);
    choice_->setObjectName(prefix + "_circuit_view");
    choice_->setAccessibleName(prefix + " circuit view — Tables or read-only Schematic");
    choice_->addItems({"Tables", "Schematic"});
    label->setBuddy(choice_);
    row->addWidget(label);
    row->addWidget(choice_);
    row->addStretch();
    layout->addLayout(row);
    pages_ = new QStackedWidget(this);
    pages_->addWidget(tables);
    layout->addWidget(pages_, 1);
    connect(owner_, &DraftView::draftEdited, this, &CircuitPresentation::schedule_refresh);
    connect(choice_, &QComboBox::currentIndexChanged, this, [this, prefix](int index) {
        // Exact text only, before hiding an active editor. No validation or execution.
        owner_->synchronize_pending_text();
        if (index == 1 && !canvas_) {
            auto* page = new QWidget(pages_);
            auto* body = new QVBoxLayout(page);
            body->setContentsMargins(0, 0, 0, 0);
            auto* note = new QLabel("Read-only current circuit draft — not solved or live "
                                    "simulation values. Edit using Tables.",
                                    page);
            note->setTextFormat(Qt::PlainText);
            note->setWordWrap(true);
            body->addWidget(note);
            canvas_ = new schematic::qt::SchematicCanvas(page);
            canvas_->setObjectName(prefix + "_schematic");
            body->addWidget(canvas_, 1);
            pages_->addWidget(page);
            // Qt moves a compound widget's whole local chain, preserving the
            // accepted canvas order when embedding it among existing host controls.
            canvas_->setFocusPolicy(Qt::StrongFocus);
            canvas_->setFocusProxy(canvas_->findChildren<QPushButton*>().front());
            QWidget::setTabOrder(choice_, canvas_);
        }
        pages_->setCurrentIndex(index);
        schedule_refresh();
    });
}
CircuitPresentation::~CircuitPresentation() {
    choice_->blockSignals(true);
    disconnect(choice_, nullptr, this, nullptr);
}
void CircuitPresentation::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    schedule_refresh();
}
void CircuitPresentation::schedule_refresh() {
    if (choice_->currentIndex() == 1 && isVisible() && !refresh_pending_) {
        refresh_pending_ = true;
        // Presentation work is separate from execution timers. The context cancels
        // this callback on destruction, including during project replacement.
        QTimer::singleShot(0, this, [this] {
            refresh_pending_ = false;
            refresh();
        });
    }
}
void CircuitPresentation::refresh() {
    if (choice_->currentIndex() != 1 || !isVisible() || !canvas_)
        return;
    try {
        owner_->synchronize_pending_text();
        auto graph = project_();
        if (graph_ && *graph_ == graph)
            return; // No rebuild for unrelated settings, navigation or progress.
        const auto selected = canvas_->selection();
        const bool installed = canvas_->setGraph(graph);
        graph_ = std::move(graph);
        if (installed && selected)
            canvas_->select(*selected); // Typed identity only; deleted entities stay cleared.
    } catch (const std::exception& error) {
        graph_.reset();
        canvas_->showFailure(QString::fromUtf8(error.what()));
    }
}
} // namespace openece::gui
