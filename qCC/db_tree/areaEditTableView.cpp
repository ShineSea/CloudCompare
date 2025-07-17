#include "areaEditTableView.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QUuid>
AreaEditTableView::AreaEditTableView(QWidget* parent)
    : BaseEditTableView(parent)
{
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
}

void AreaEditTableView::onAdd()
{
	AreaInfoTableModel* model = qobject_cast<AreaInfoTableModel*>(this->model());
    if (!model) return;
    AreaLabelInfo info;
    info.areaId=QUuid::createUuid().toString().remove("{").remove("}").remove('-');
    model->addAreaInfo(info);
}

