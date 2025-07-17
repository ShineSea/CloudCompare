#pragma once
#include "baseEditTableView.h"
#include "FactoryInfoTableModel.h"
#include <QMenu>
#include <QAction>

class FactoryEditTableView : public BaseEditTableView
{
    Q_OBJECT
public:
    FactoryEditTableView(QWidget* parent = nullptr);
private slots:
    void onAdd();
};


