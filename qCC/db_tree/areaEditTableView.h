#pragma once
#include "baseEditTableView.h"
#include "areaInfoTableModel.h"
#include <QMenu>
#include <QAction>

class AreaEditTableView : public BaseEditTableView
{
    Q_OBJECT
public:
    AreaEditTableView(QWidget* parent = nullptr);

private slots:
    void onAdd();
};
