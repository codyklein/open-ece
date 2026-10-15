#include "project_workspace.hpp"
#include "circuits_workspace.hpp"
#include "communications_view.hpp"
#include "digital_workspace.hpp"
#include "signals_dsp_view.hpp"
#include "transient_view.hpp"
#include <QAbstractSpinBox>
#include <QApplication>
#include <QFocusFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPointer>
#include <QProxyStyle>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
namespace openece::gui {
namespace {
// Give the frame room outside the editor and paint above native control borders.
// This style is local to the indicator; all controls retain their native style.
class FocusFrameStyle final : public QProxyStyle {
  public:
    int pixelMetric(PixelMetric metric, const QStyleOption* option,
                    const QWidget* widget) const override {
        if (metric == PM_FocusFrameHMargin || metric == PM_FocusFrameVMargin)
            return 5;
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
                  QStyleHintReturn* data) const override {
        if (hint == SH_FocusFrame_AboveWidget)
            return true;
        if (hint == SH_FocusFrame_Mask)
            return false; // Do not clip the wider ring to a native one-pixel mask.
        return QProxyStyle::styleHint(hint, option, widget, data);
    }
};
// Qt's QFocusFrame owns positioning/scrolling and never takes keyboard input.
class CurrentFocusFrame final : public QFocusFrame {
  public:
    explicit CurrentFocusFrame(QWidget* parent) : QFocusFrame(parent) {
        setObjectName("keyboard_focus_indicator");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* frame_style = new FocusFrameStyle;
        frame_style->setParent(this);
        setStyle(frame_style);
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setPen(QPen(Qt::white, 5));
        painter.drawRect(rect().adjusted(3, 3, -4, -4));
        painter.setPen(QPen(QColor(0, 85, 210), 3));
        painter.drawRect(rect().adjusted(3, 3, -4, -4));
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
        connect(qApp, &QApplication::focusChanged, this, [this, focus](QWidget* old, QWidget* now) {
            // Spin boxes keep their selected text after Tab. Remove that inactive
            // selection so it cannot masquerade as focus. Deselect never commits,
            // parses or changes the raw editor buffer or persisted draft.
            if (old && isAncestorOf(old) && old != now) {
                auto* spin = qobject_cast<QAbstractSpinBox*>(old);
                if (!spin)
                    spin = qobject_cast<QAbstractSpinBox*>(old->parentWidget());
                if (spin && now != spin && (!now || !spin->isAncestorOf(now)))
                    if (auto* editor = spin->findChild<QLineEdit*>())
                        editor->deselect();
            }
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
    for (auto* view : findChildren<TransientView*>())
        view->stop_execution();
    for (auto* timer : findChildren<QTimer*>())
        timer->stop();
}
project::ProjectSnapshot ProjectWorkspace::capture() {
    synchronize_pending_text();
    return snapshot_;
}
} // namespace openece::gui
