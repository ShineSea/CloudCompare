#include "factoryEditDlg.h"
#include "factoryEditTableView.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>
FactoryEditDlg::FactoryEditDlg(QWidget* parent)
    : QDialog(parent)
{
    m_tableView = new FactoryEditTableView(this);
	m_model = new FactoryInfoTableModel(this);
	m_tableView->setModel(m_model);
	m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_tableView);
	layout->addWidget(buttonBox);
    setLayout(layout);
    setWindowTitle("厂站信息编辑");
    resize(800, 600);

	connect(buttonBox, &QDialogButtonBox::accepted, this, &FactoryEditDlg::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &FactoryEditDlg::reject);
}

void FactoryEditDlg::addFactoryInfo(const FactoryLabelInfo& info)
{
	m_model->addFactoryInfo(info);
}

QList<FactoryLabelInfo> FactoryEditDlg::getAllFactoryInfo() const
{
	return m_model->getAll();
}

