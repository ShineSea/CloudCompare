#ifndef CC_LABEL_INFO_EDIT_DIALOG_HEADER
#define CC_LABEL_INFO_EDIT_DIALOG_HEADER

#include <QDialog>

namespace Ui {
	class LabelInfoEditDlg;
}
struct LabelDeviceInfo;
class labelInfoEditDlg : public QDialog
{
	Q_OBJECT

public:
	explicit labelInfoEditDlg(QWidget* parent = nullptr);
	~labelInfoEditDlg();

    void setLabelInfo(const LabelDeviceInfo& labelInfo);
    LabelDeviceInfo getLabelInfo() const;
private:
	Ui::LabelInfoEditDlg* m_ui;
};

#endif

