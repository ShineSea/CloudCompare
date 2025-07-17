#pragma once
#include "baseEditTableView.h"
#include "IntervalInfoTableModel.h"
#include <QMenu>
#include <QAction>

class IntervalEditTableView : public BaseEditTableView
{
    Q_OBJECT
public:
    IntervalEditTableView(QWidget* parent = nullptr);

private slots:
    void onAdd();
};
