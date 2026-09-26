#include "draft_view.hpp"
#include <QComboBox>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QTableWidget>
namespace openece::gui {
namespace {
class TextDelegate final : public QStyledItemDelegate {
    std::function<void(int, int, const QString&)> changed_;

  public:
    TextDelegate(QTableWidget* parent, std::function<void(int, int, const QString&)> changed)
        : QStyledItemDelegate(parent), changed_(std::move(changed)) {}
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&,
                          const QModelIndex& index) const override {
        auto* editor = new QLineEdit(parent);
        editor->setMaxLength(static_cast<int>(project::limits::cell_bytes));
        QPersistentModelIndex persistent(index);
        connect(editor, &QLineEdit::textEdited, editor, [this, persistent](const QString& text) {
            if (persistent.isValid())
                changed_(persistent.row(), persistent.column(), text);
        });
        return editor;
    }
    void destroyEditor(QWidget* editor, const QModelIndex& index) const override {
        editor->setProperty("draftIndex", QVariant{});
        QStyledItemDelegate::destroyEditor(editor, index);
    }
    bool eventFilter(QObject* object, QEvent* event) override {
        if (event->type() == QEvent::KeyPress &&
            static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            auto* edit = qobject_cast<QLineEdit*>(object);
            auto index = edit->property("draftIndex").value<QPersistentModelIndex>();
            if (index.isValid())
                changed_(index.row(), index.column(), index.data(Qt::EditRole).toString());
        }
        return QStyledItemDelegate::eventFilter(object, event);
    }
    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        editor->setProperty("draftIndex", QVariant::fromValue(QPersistentModelIndex(index)));
        QStyledItemDelegate::setEditorData(editor, index);
    }
};
} // namespace
void DraftView::bind_text(QLineEdit* widget, std::string& field) {
    widget->setText(qt_text(field));
    auto sync = [this, widget, &field] { edit(field, draft_text(widget->text())); };
    connect(widget, &QLineEdit::textChanged, this, sync);
    synchronizers_.push_back(sync);
}
void DraftView::bind_spin(QSpinBox* base, std::string& field) {
    auto* spin = static_cast<DraftInt*>(base);
    spin->restore_text(field);
    auto sync = [this, spin, &field] { edit(field, draft_text(spin->raw_text())); };
    connect(spin->editor(), &QLineEdit::textChanged, this, sync);
    connect(base, &QSpinBox::textChanged, this, sync);
    synchronizers_.push_back(sync);
}
void DraftView::bind_spin(QDoubleSpinBox* base, std::string& field) {
    auto* spin = static_cast<DraftDouble*>(base);
    spin->restore_text(field);
    auto sync = [this, spin, &field] { edit(field, draft_text(spin->raw_text())); };
    connect(spin->editor(), &QLineEdit::textChanged, this, sync);
    connect(base, &QDoubleSpinBox::textChanged, this, sync);
    synchronizers_.push_back(sync);
}
void DraftView::bind_choice(QComboBox* box, std::string& field, const QStringList& tokens) {
    {
        const QSignalBlocker block(box);
        box->setCurrentIndex(static_cast<int>(tokens.indexOf(qt_text(field))));
    }
    connect(box, &QComboBox::currentIndexChanged, this, [this, box, &field, tokens] {
        const int i = box->currentIndex();
        if (i >= 0 && i < tokens.size())
            edit(field, draft_text(tokens[i]));
    });
}
void DraftView::bind_tabs(QTabWidget* tabs, std::string& field, const QStringList& tokens) {
    tabs->setCurrentIndex(static_cast<int>(tokens.indexOf(qt_text(field))));
    connect(tabs, &QTabWidget::currentChanged, this, [this, &field, tokens](int index) {
        if (index >= 0 && index < tokens.size())
            edit(field, draft_text(tokens[index]));
    });
}
void DraftView::bind_table(QTableWidget* table,
                           std::function<void(int, int, const QString&)> changed) {
    table->setItemDelegate(new TextDelegate(table, changed));
    synchronizers_.push_back([table, changed] {
        for (auto* editor : table->findChildren<QLineEdit*>()) {
            auto index = editor->property("draftIndex").value<QPersistentModelIndex>();
            if (index.isValid())
                changed(index.row(), index.column(), editor->text());
        }
    });
}
void DraftView::synchronize_pending_text() {
    for (auto& sync : synchronizers_)
        sync();
}
} // namespace openece::gui
