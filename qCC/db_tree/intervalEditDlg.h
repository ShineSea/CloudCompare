#pragma once
#include <QDialog>
#include "intervalInfoTableModel.h"
class IntervalEditTableView;

class IntervalEditDlg : public QDialog
{
    Q_OBJECT
public:
    explicit IntervalEditDlg(QWidget* parent = nullptr);

	void addIntervalInfo(const IntervalLabelInfo& info);
	QList<IntervalLabelInfo> getAllIntervalInfo() const;
private:
    IntervalEditTableView* m_tableView;
	IntervalInfoTableModel *m_model;
};
