#ifndef CC_LABEL_PATH_DIALOG_HEADER
#define CC_LABEL_PATH_DIALOG_HEADER

// GUI
#include <ui_labelPathDlg.h>

// Local
#include "ccPointPickingGenericInterface.h"

// qCC_db
#include <ccHObject.h>

class cc2DLabel;

//! Dialog/interactor to graphically pick a list of points
/** Options let the user export the list to an ASCII file, a new cloud, a polyline, etc.
 **/
class ccLabelPathDlg
    : public ccPointPickingGenericInterface
    , public Ui::LabelPathDlg {
    Q_OBJECT

public:
    //! Default constructor
    explicit ccLabelPathDlg(ccPickingHub *pickingHub, QWidget *parent);

    //! Associates dialog with a cloud or a mesh
    void linkWithEntity(ccHObject *entity);

	static ccHObject*  getLabelPathGroup(ccHObject* entity);

protected:
    //! Applies changes and exit
    void applyAndExit();
    //! Cancels process and exit
    void cancelAndExit();
    //! Removes last inserted point from list
    void removeLastEntry();
    //! Redraw window when marker size changes
    void markerSizeChanged(int);
    //! Updates point list widget
    void updateList();

protected:
    // inherited from ccPointPickingGenericInterface
    void processPickedPoint(const PickedItem &picked) override;

    //! Gets current (visible) picked points from the associated cloud
    unsigned getPickedPoints(std::vector<cc2DLabel *> &pickedPoints);

    //! Export format
    /** See exportToASCII.
     **/
    enum ExportFormat { PLP_ASCII_EXPORT_XYZ, PLP_ASCII_EXPORT_IXYZ, PLP_ASCII_EXPORT_GXYZ, PLP_ASCII_EXPORT_LXYZ };

    //! Exports list to an ASCII file
    void exportToASCII(ExportFormat format);

    //! Associated cloud or mesh
    ccHObject *m_associatedEntity;

    //! Last existing label unique ID on load
    unsigned m_lastPreviousID;

	//! last pathId
	unsigned m_lastPathID;
    //! Ordered labels container
    ccHObject *m_orderedLabelsContainer;
    //! Existing picked points that the user wants to delete (for proper "cancel" mechanism)
    ccHObject::Container m_toBeDeleted;
    //! New picked points that the user has selected (for proper "cancel" mechanism)
    ccHObject::Container m_toBeAdded;
};

#endif
