#include "intervalEditTableView.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QHeaderView>
#include "intervalInfoTableModel.h"
#include <QUuid>

IntervalEditTableView::IntervalEditTableView(QWidget* parent)
    : BaseEditTableView(parent)
{
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
}



void IntervalEditTableView::onAdd()
{
   	IntervalInfoTableModel* model = qobject_cast<IntervalInfoTableModel*>(this->model());
    if (!model) return;
    IntervalLabelInfo info;
    info.intervalId=QUuid::createUuid().toString().remove("{").remove("}").remove('-');
    model->addIntervalInfo(info);
}


