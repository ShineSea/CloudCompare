#include "intervalEditDlg.h"
#include "intervalEditTableView.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>

IntervalEditDlg::IntervalEditDlg(QWidget* parent)
    : QDialog(parent)
{
    m_tableView = new IntervalEditTableView(this);
	m_model = new IntervalInfoTableModel(this);
	m_tableView->setModel(m_model);
	m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_tableView);
	layout->addWidget(buttonBox);
    setLayout(layout);
    setWindowTitle("间隔信息编辑");
    resize(800, 600);

	connect(buttonBox, &QDialogButtonBox::accepted, this, &IntervalEditDlg::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &IntervalEditDlg::reject);
}

void IntervalEditDlg::addIntervalInfo(const IntervalLabelInfo& info)
{
	m_model->addIntervalInfo(info);
}

QList<IntervalLabelInfo> IntervalEditDlg::getAllIntervalInfo() const
{
	return m_model->getAll();

}
