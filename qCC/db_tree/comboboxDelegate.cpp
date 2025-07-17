#include "comboboxDelegate.h"
#include <QComboBox>

// ComboBoxDelegate 实现
ComboBoxDelegate::ComboBoxDelegate(const QStringList& items, QObject* parent)
    : QStyledItemDelegate(parent), m_items(items) {}

QWidget* ComboBoxDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const
{
    QComboBox* editor = new QComboBox(parent);
    editor->addItems(m_items);
    return editor;
}

void ComboBoxDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QComboBox* comboBox = static_cast<QComboBox*>(editor);
    int idx = comboBox->findText(value);
    if (idx >= 0)
        comboBox->setCurrentIndex(idx);
}

void ComboBoxDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    QComboBox* comboBox = static_cast<QComboBox*>(editor);
    model->setData(index, comboBox->currentText(), Qt::EditRole);
}

void ComboBoxDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex&) const
{
    editor->setGeometry(option.rect);
}
