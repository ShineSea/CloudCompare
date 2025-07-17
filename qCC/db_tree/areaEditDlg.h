#pragma once
#include <QDialog>
#include "areaInfoTableModel.h"
class AreaEditTableView;

class AreaEditDlg : public QDialog
{
    Q_OBJECT
public:
    explicit AreaEditDlg(QWidget* parent = nullptr);

	void addAreaInfo(const AreaLabelInfo& info);

	QList<AreaLabelInfo> getAllAreaInfo() const;
private:
    AreaEditTableView* m_tableView;
	AreaInfoTableModel *m_model;
};
