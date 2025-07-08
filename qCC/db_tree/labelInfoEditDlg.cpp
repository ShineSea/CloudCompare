#include "labelInfoEditDlg.h"
#include "ui_labelInfoEditDlg.h"
#include "ccPolyline.h"
labelInfoEditDlg::labelInfoEditDlg(QWidget* parent)
	: QDialog(parent)
	, m_ui(new Ui::LabelInfoEditDlg())
{
	m_ui->setupUi(this);
	connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &labelInfoEditDlg::accept);
	connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &labelInfoEditDlg::reject);
}

labelInfoEditDlg::~labelInfoEditDlg()
{
	delete m_ui;
}

void labelInfoEditDlg::setLabelInfo(const LabelDeviceInfo& labelInfo)
{
	m_ui->lineEditDeviceId->setText(labelInfo.deviceId);
	m_ui->lineEditDeviceName->setText(labelInfo.deviceName);
}

LabelDeviceInfo labelInfoEditDlg::getLabelInfo() const
{
	LabelDeviceInfo labelInfo;
	labelInfo.deviceId = m_ui->lineEditDeviceId->text();
	labelInfo.deviceName = m_ui->lineEditDeviceName->text();
	return labelInfo;
}
