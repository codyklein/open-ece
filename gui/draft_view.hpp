#pragma once
#include <QCloseEvent>
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <openece/project/model.hpp>
class QTableWidget;
class QComboBox;
namespace openece::gui {
inline QString qt_text(const std::string& text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}
inline std::string draft_text(const QString& text) { return text.toUtf8().toStdString(); }
template <class T> class DraftOwner {
    std::unique_ptr<T> owned_;
    T* model_;

  public:
    DraftOwner(T* external, T initial)
        : owned_(external ? nullptr : std::make_unique<T>(std::move(initial))),
          model_(external ? external : owned_.get()) {}
    T& get() { return *model_; }
    const T& get() const { return *model_; }
};
// Widgets edit one owned/borrowed draft. Results and runtime state never enter this base.
class DraftView : public QWidget {
    Q_OBJECT
  public:
    explicit DraftView(QWidget* parent = nullptr) : QWidget(parent) {}
    virtual void synchronize_pending_text();
  Q_SIGNALS:
    void draftEdited();

  protected:
    bool restoring_ = true;
    template <class T> void edit(T& field, T value) {
        if (!restoring_ && field != value) {
            field = std::move(value);
            Q_EMIT draftEdited();
        }
    }
    void bind_text(QLineEdit*, std::string&);
    void bind_spin(QSpinBox*, std::string&);
    void bind_spin(QDoubleSpinBox*, std::string&);
    void bind_choice(QComboBox*, std::string&, const QStringList&);
    void bind_tabs(QTabWidget*, std::string&, const QStringList&);
    void bind_table(QTableWidget*, std::function<void(int, int, const QString&)>);
    void edited() {
        if (!restoring_)
            Q_EMIT draftEdited();
    }

  private:
    std::vector<std::function<void()>> synchronizers_;
};
// Qt's default spin boxes interpret pending input on focus loss/hide/close. Draft
// editors retain that exact buffer; only execution and explicit stepping parse it.
template <class Base> class DraftSpin final : public Base {
  public:
    explicit DraftSpin(QWidget* p = nullptr) : Base(p) {
        this->setLocale(QLocale::c());
        this->lineEdit()->setMaxLength(160); // 128 draft units plus display suffix
    }
    QString raw_text() const {
        auto s = this->lineEdit()->text();
        if (!this->prefix().isEmpty() && s.startsWith(this->prefix()))
            s.remove(0, this->prefix().size());
        if (!this->suffix().isEmpty() && s.endsWith(this->suffix()))
            s.chop(this->suffix().size());
        return s;
    }
    void restore_text(const std::string& s) {
        const QSignalBlocker block(this->lineEdit());
        this->lineEdit()->setText(this->prefix() + qt_text(s) + this->suffix());
    }
    QLineEdit* editor() const { return this->lineEdit(); }
    QValidator::State validate(QString& text, int& pos) const override {
        auto copy = text;
        int position = pos;
        auto state = Base::validate(copy, position);
        return state == QValidator::Invalid ? QValidator::Intermediate : state;
    }
    void fixup(QString&) const override {}
    void stepBy(int steps) override {
        bool ok = false;
        auto locale = QLocale::c();
        locale.setNumberOptions(QLocale::RejectGroupSeparator);
        double v = locale.toDouble(raw_text(), &ok);
        if (!ok || !std::isfinite(v) || v < this->minimum() || v > this->maximum())
            return;
        if constexpr (std::is_same_v<Base, QSpinBox>) {
            if (std::floor(v) != v)
                return;
            this->setValue(
                static_cast<int>(std::clamp(v + double(steps) * this->singleStep(),
                                            double(this->minimum()), double(this->maximum()))));
        } else
            this->setValue(std::clamp(v + double(steps) * this->singleStep(), this->minimum(),
                                      this->maximum()));
    }

  protected:
    void focusOutEvent(QFocusEvent* e) override {
        QWidget::focusOutEvent(e);
        Q_EMIT this->editingFinished();
    }
    void hideEvent(QHideEvent* e) override { QWidget::hideEvent(e); }
    void showEvent(QShowEvent* e) override { QWidget::showEvent(e); }
    void closeEvent(QCloseEvent* e) override { QWidget::closeEvent(e); }
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            Q_EMIT this->editingFinished();
            e->accept();
        } else
            Base::keyPressEvent(e);
    }
};
using DraftInt = DraftSpin<QSpinBox>;
using DraftDouble = DraftSpin<QDoubleSpinBox>;
inline int draft_value(QSpinBox* spin) {
    auto* d = static_cast<DraftInt*>(spin);
    bool ok = false;
    auto locale = QLocale::c();
    locale.setNumberOptions(QLocale::RejectGroupSeparator);
    const auto v = locale.toInt(d->raw_text(), &ok);
    if (!ok || v < spin->minimum() || v > spin->maximum())
        throw std::invalid_argument("Invalid pending count or count outside the supported range.");
    return v;
}
inline double draft_value(QDoubleSpinBox* spin) {
    auto* d = static_cast<DraftDouble*>(spin);
    bool ok = false;
    auto locale = QLocale::c();
    locale.setNumberOptions(QLocale::RejectGroupSeparator);
    const auto v = locale.toDouble(d->raw_text(), &ok);
    if (!ok || !std::isfinite(v) || v < spin->minimum() || v > spin->maximum())
        throw std::invalid_argument(
            "Invalid pending number or number outside the supported range.");
    return v;
}
} // namespace openece::gui
