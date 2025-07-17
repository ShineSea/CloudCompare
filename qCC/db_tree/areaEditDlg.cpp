#include "areaEditDlg.h"
#include "areaEditTableView.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>

AreaEditDlg::AreaEditDlg(QWidget* parent)
    : QDialog(parent)
{
    m_tableView = new AreaEditTableView(this);
	m_model = new AreaInfoTableModel(this);
	m_tableView->setModel(m_model);
	m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_tableView);
	layout->addWidget(buttonBox);
    setLayout(layout);
    setWindowTitle("区域信息编辑");
    resize(800, 600);

	connect(buttonBox, &QDialogButtonBox::accepted, this, &AreaEditDlg::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &AreaEditDlg::reject);
}


void AreaEditDlg::addAreaInfo(const AreaLabelInfo& info)
{
	m_model->addAreaInfo(info);
}

QList<AreaLabelInfo> AreaEditDlg::getAllAreaInfo() const
{
	return m_model->getAll();
}
