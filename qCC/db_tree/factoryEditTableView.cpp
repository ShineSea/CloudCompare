#include "factoryEditTableView.h"
#include <QHeaderView>
#include <QUuid>
#include "comboboxDelegate.h"
FactoryEditTableView::FactoryEditTableView(QWidget* parent)
    : BaseEditTableView(parent)
{
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);

	QStringList voltageLevels;
	voltageLevels<<"10kV"<<"35kV"<<"66kV"<<"110kV"<<"220kV"<<"330kV"<<"500kV"<<"750kV"<<"1000kV"
	<<"±800kV"<<"±500kV";

	ComboBoxDelegate* delegate = new ComboBoxDelegate(voltageLevels, this);
	setItemDelegateForColumn(2, delegate);
}

void FactoryEditTableView::onAdd()
{
    FactoryInfoTableModel* model = qobject_cast<FactoryInfoTableModel*>(this->model());
    if (!model) return;
    FactoryLabelInfo info;
    info.factoryId=QUuid::createUuid().toString().remove("{").remove("}").remove('-');
    model->addFactoryInfo(info);
}





