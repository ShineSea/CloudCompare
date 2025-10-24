#pragma once

//##########################################################################
//#                                                                        #
//#                              CLOUDCOMPARE                              #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 or later of the License.      #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#          COPYRIGHT: EDF R&D / TELECOM ParisTech (ENST-TSI)             #
//#                                                                        #
//##########################################################################

//Local
#include "ccOverlayDialog.h"
#include "ccPickingListener.h"

//qCC_db
#include <ccGenericGLDisplay.h>

//system
#include <vector>

#include "ccHObject.h"
class ccPolyline;
class ccPointCloud;
class ccGLWindowInterface;
class ccPickingHub;
class ccDBRoot;
namespace Ui
{
	class LabelAreaDlg;
}

//! Graphical Polyline Tracing tool
class ccLabelAreaTool : public ccOverlayDialog, public ccPickingListener
{
	Q_OBJECT

public:
	//! Default constructor
	explicit ccLabelAreaTool(ccPickingHub* pickingHub, QWidget* parent);
	//! Destructor
	virtual ~ccLabelAreaTool();

	//inherited from ccOverlayDialog
	virtual bool linkWith(ccGLWindowInterface* win) override;
	virtual bool start() override;
	virtual void stop(bool accepted) override;

protected:

	void apply();
	void cancel();
	void exportLine();
	void undo();
	inline void resetLine() { restart(true); }

	void closePolyLine(int x = 0, int y = 0); //arguments for compatibility with ccGlWindow::rightButtonClicked signal
	void updatePolyLineTip(int x, int y, Qt::MouseButtons buttons);
	void updatePolyTipFirstP();

	void onWidthSizeChanged(int);

	//! To capture overridden shortcuts (pause button, etc.)
	void onShortcutTriggered(int);

	//! Inherited from ccPickingListener
	virtual void onItemPicked(const PickedItem& pi) override;

protected:

	//! Restarts the edition mode
	void restart(bool reset);

	void updateLabelInfos();

	void updateFactoryComboBox();

	void updateAreaComboBox();

	void updateIntervalComboBox();
	//! Viewport parameters (used for picking)
	struct SegmentGLParams
	{
		SegmentGLParams() {}
		SegmentGLParams(ccGenericGLDisplay* display, int x, int y);
		ccGLCameraParameters params;
		CCVector2d clickPos;
	};

	//! 2D polyline (for the currently edited part)
	ccPolyline* m_polyTip;
	//! 2D polyline vertices
	ccPointCloud* m_polyTipVertices;

	//!dbRoot
	ccDBRoot* m_dbRoot;

	//! 3D polyline
	ccPolyline* m_poly3D;
	//! 3D polyline vertices
	ccPointCloud* m_poly3DVertices;

	//! Viewport parameters use to draw each segment of the polyline
	std::vector<SegmentGLParams> m_segmentParams;
	//! Current process state
	bool m_done;

	//! Picking hub
	ccPickingHub* m_pickingHub;

	Ui::LabelAreaDlg* m_ui;

	QMap<QString, ccHObject*> m_intervalMap;
	QList<FactoryLabelInfo> m_factoryInfoList;
	QMap<QString, QList<AreaLabelInfo>> m_areaInfoMap;
	QMap<QString, QList<IntervalLabelInfo>> m_intervalInfoMap;

	static QString m_lastFactoryId;
	static QString m_lastAreaId;
	static QString m_lastIntervalId;
};
