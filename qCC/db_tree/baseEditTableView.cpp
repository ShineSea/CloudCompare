#include "baseEditTableView.h"
#include <QInputDialog>
#include <QMessageBox>

BaseEditTableView::BaseEditTableView(QWidget* parent)
    : QTableView(parent)
{
    m_menu = new QMenu(this);
    m_addAction = m_menu->addAction("新增");
    m_deleteAction = m_menu->addAction("删除");

    connect(m_deleteAction, &QAction::triggered, this, &BaseEditTableView::onDelete);
	connect(m_addAction, &QAction::triggered, this, &BaseEditTableView::onAdd);

    setContextMenuPolicy(Qt::DefaultContextMenu);
}


void BaseEditTableView::contextMenuEvent(QContextMenuEvent* event)
{
    QModelIndex index = indexAt(event->pos());
    m_deleteAction->setEnabled(index.isValid());
    m_menu->exec(event->globalPos());
}


void BaseEditTableView::onDelete()
{
    QModelIndex index = currentIndex();
    if (!index.isValid()) return;

    int row = index.row();
    this->model()->removeRow(row);
}


void BaseEditTableView::onAdd()
{

}
