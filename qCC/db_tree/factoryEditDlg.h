#pragma once
#include <QDialog>
#include "FactoryInfoTableModel.h"
#include "ccHObject.h"
class FactoryEditTableView;

class FactoryEditDlg : public QDialog
{
    Q_OBJECT
public:
    explicit FactoryEditDlg(QWidget* parent = nullptr);

	void addFactoryInfo(const FactoryLabelInfo& info);

	QList<FactoryLabelInfo> getAllFactoryInfo() const;

private:
    FactoryEditTableView* m_tableView;
	FactoryInfoTableModel* m_model;
};
