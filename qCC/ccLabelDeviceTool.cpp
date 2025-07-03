// ##########################################################################
// #                                                                        #
// #                              CLOUDCOMPARE                              #
// #                                                                        #
// #  This program is free software; you can redistribute it and/or modify  #
// #  it under the terms of the GNU General Public License as published by  #
// #  the Free Software Foundation; version 2 or later of the License.      #
// #                                                                        #
// #  This program is distributed in the hope that it will be useful,       #
// #  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
// #  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
// #  GNU General Public License for more details.                          #
// #                                                                        #
// #          COPYRIGHT: EDF R&D / TELECOM ParisTech (ENST-TSI)             #
// #                                                                        #
// ##########################################################################

#include "ccLabelDeviceTool.h"
#include "ui_labelDeviceDlg.h"

// Local
#include "mainwindow.h"
#include "ccReservedIDs.h"

// common
#include <ccPickingHub.h>

// qCC_db
#include <ccPolyline.h>
#include <ccPointCloud.h>
#include <ccProgressDialog.h>

// System
#include <cassert>
#include <cmath>

#include <QShortcut>
#include <QHeaderView>

#include "db_tree/ccDBRoot.h"

#ifdef CC_CORE_LIB_USES_TBB
#include <tbb/parallel_for.h>
#endif

#if defined(_OPENMP)
//OpenMP
#include <omp.h>
#endif

ccLabelDeviceTool::SegmentGLParams::SegmentGLParams(ccGenericGLDisplay *display, int x, int y)
{
	if (display)
	{
		display->getGLCameraParameters(params);
		QPointF pos2D = display->toCornerGLCoordinates(x, y);
		clickPos = CCVector2d(pos2D.x(), pos2D.y());
	}
}

ccLabelDeviceTool::ccLabelDeviceTool(ccPickingHub *pickingHub, QWidget *parent)
	: ccOverlayDialog(parent), m_polyTip(nullptr), m_polyTipVertices(nullptr), m_poly3D(nullptr), m_poly3DVertices(nullptr), m_done(false), m_pickingHub(pickingHub), m_ui(new Ui::LabelDeviceDlg), m_dbRoot(MainWindow::TheInstance()->db())
{
	assert(pickingHub);

	m_ui->setupUi(this);
	setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);

	connect(m_ui->saveToolButton, &QToolButton::clicked, this, &ccLabelDeviceTool::exportLine);
	connect(m_ui->resetToolButton, &QToolButton::clicked, this, &ccLabelDeviceTool::resetLine);
	connect(m_ui->continueToolButton, &QToolButton::clicked, this, &ccLabelDeviceTool::continueEdition);
	connect(m_ui->validButton, &QToolButton::clicked, this, &ccLabelDeviceTool::apply);
	connect(m_ui->cancelButton, &QToolButton::clicked, this, &ccLabelDeviceTool::cancel);

	// add shortcuts
	addOverriddenShortcut(Qt::Key_Escape); // escape key for the "cancel" button
	addOverriddenShortcut(Qt::Key_Return); // return key for the "apply" button
	connect(this, &ccLabelDeviceTool::shortcutTriggered, this, &ccLabelDeviceTool::onShortcutTriggered);

	QShortcut *undoShortcut = new QShortcut(QKeySequence::Undo, this);
	undoShortcut->setContext(Qt::ApplicationShortcut);
	connect(undoShortcut, &QShortcut::activated, this, &ccLabelDeviceTool::undo);

	m_polyTipVertices = new ccPointCloud("Tip vertices", static_cast<unsigned>(ReservedIDs::TRACE_POLYLINE_TOOL_POLYLINE_TIP_VERTICES));
	m_polyTipVertices->reserve(2);
	m_polyTipVertices->addPoint(CCVector3(0, 0, 0));
	m_polyTipVertices->addPoint(CCVector3(1, 1, 1));
	m_polyTipVertices->setEnabled(false);

	m_polyTip = new ccPolyline(m_polyTipVertices, static_cast<unsigned>(ReservedIDs::TRACE_POLYLINE_TOOL_POLYLINE_TIP));
	m_polyTip->showVertices(true);
	m_polyTip->setVertexMarkerWidth(8);
	m_polyTip->setForeground(true);
	m_polyTip->setTempColor(ccColor::orange);
	m_polyTip->set2DMode(true);
	m_polyTip->reserve(2);
	m_polyTip->addPointIndex(0, 2);
	m_polyTip->setWidth(2);
	m_polyTip->addChild(m_polyTipVertices);

	m_ui->validButton->setEnabled(false);
	m_ui->tableDeviceInfo->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

ccLabelDeviceTool::~ccLabelDeviceTool()
{
	// m_polyTipVertices is already a child of m_polyTip
	if (m_polyTip)
		delete m_polyTip;
	m_polyTip = nullptr;

	// m_poly3DVertices is already a child of m_poly3D
	if (m_poly3D)
		delete m_poly3D;
	m_poly3D = nullptr;

	clearPolyDevices();
	delete m_ui;
	m_ui = nullptr;
}

void ccLabelDeviceTool::onShortcutTriggered(int key)
{
	switch (key)
	{
	case Qt::Key_Return:
		apply();
		return;

	case Qt::Key_Escape:
		cancel();
		return;

	default:
		// nothing to do
		break;
	}
}

bool ccLabelDeviceTool::linkWith(ccGLWindowInterface *win)
{
	assert(m_polyTip);
	assert(!m_poly3D || !win);

	ccGLWindowInterface *oldWin = m_associatedWin;

	if (!ccOverlayDialog::linkWith(win))
	{
		return false;
	}

	if (oldWin)
	{
		oldWin->removeFromOwnDB(m_polyTip);
		oldWin->signalEmitter()->disconnect(this);

		if (m_polyTip)
			m_polyTip->setDisplay(nullptr);
	}

	if (m_associatedWin)
	{
		connect(m_associatedWin->signalEmitter(), &ccGLWindowSignalEmitter::rightButtonClicked, this, &ccLabelDeviceTool::closePolyLine);
		connect(m_associatedWin->signalEmitter(), &ccGLWindowSignalEmitter::mouseMoved, this, &ccLabelDeviceTool::updatePolyLineTip);
	}

	return true;
}

static int s_defaultPickingRadius = 1;
static int s_overSamplingCount = 1;
bool ccLabelDeviceTool::start()
{
	assert(m_polyTip);
	assert(!m_poly3D);

	if (!m_associatedWin)
	{
		ccLog::Warning("[Trace Polyline Tool] No associated window!");
		return false;
	}

	updateIntervalGroupMap();

	m_associatedWin->setUnclosable(true);
	m_associatedWin->addToOwnDB(m_polyTip);
	if (m_pickingHub)
	{
		m_pickingHub->removeListener(this);
	}
	m_associatedWin->setPickingMode(ccGLWindowInterface::NO_PICKING);
	m_associatedWin->setInteractionMode(ccGLWindowInterface::MODE_TRANSFORM_CAMERA | ccGLWindowInterface::INTERACT_SIG_RB_CLICKED | ccGLWindowInterface::INTERACT_CTRL_PAN | ccGLWindowInterface::INTERACT_SIG_MOUSE_MOVED);
	m_associatedWin->setWindowCursor(Qt::CrossCursor);

	resetLine(); // to reset the GUI

	return ccOverlayDialog::start();
}

void ccLabelDeviceTool::stop(bool accepted)
{
	assert(m_polyTip);

	if (m_pickingHub)
	{
		m_pickingHub->removeListener(this);
	}

	if (m_associatedWin)
	{
		m_associatedWin->displayNewMessage("Polyline tracing [OFF]",
										   ccGLWindowInterface::UPPER_CENTER_MESSAGE,
										   false,
										   2,
										   ccGLWindowInterface::MANUAL_SEGMENTATION_MESSAGE);

		m_associatedWin->setUnclosable(false);
		m_associatedWin->removeFromOwnDB(m_polyTip);
		m_associatedWin->setInteractionMode(ccGLWindowInterface::MODE_TRANSFORM_CAMERA);
		m_associatedWin->setWindowCursor(Qt::ArrowCursor);
	}

	ccOverlayDialog::stop(accepted);
}

void ccLabelDeviceTool::undo()
{
	m_poly3D->setEnabled(false);
	m_polyTip->setEnabled(false);
	if (m_poly3DVertices)
	{
		std::vector<CCVector3> points;
		for (int i = 0; i < m_poly3DVertices->size() - 1; ++i)
		{
			points.push_back(*(m_poly3DVertices->getPoint(i)));
		}
		m_poly3DVertices->reset();
		m_poly3DVertices->reserve(static_cast<unsigned>(points.size()));
		for (const auto &point : points)
		{
			m_poly3DVertices->addPoint(point);
		}
	}
	if (m_poly3D)
	{
		int size = std::min(unsigned int(4), m_poly3DVertices->size());
		m_poly3D->resize(size);
		if (size > 0)
			m_poly3D->addPointIndex(0, size);
	}
	if (m_poly3DVertices->size() >= 1)
	{
		updatePolyTipFirstP();
		if (!m_done)
		{
			m_polyTip->setEnabled(true);
		}
	}
	m_poly3D->setEnabled(true);
	updatePolyDevices();
	m_associatedWin->redraw(false, false);
}

void ccLabelDeviceTool::updatePolyLineTip(int x, int y, Qt::MouseButtons buttons)
{
	if (!m_associatedWin)
	{
		assert(false);
		return;
	}

	if (buttons != Qt::NoButton)
	{
		// nothing to do (just hide the tip)
		if (m_polyTip->isEnabled())
		{
			m_polyTip->setEnabled(false);
			m_associatedWin->redraw(true, false);
		}
		return;
	}

	if (!m_poly3DVertices || m_poly3DVertices->size() == 0)
	{
		// there should be at least one point already picked!
		return;
	}

	if (m_done)
	{
		// when it is done do nothing
		return;
	}

	assert(m_polyTip && m_polyTipVertices && m_polyTipVertices->size() == 2);
	// just in case (e.g. if the view has been rotated or zoomed)
	// we also update the first vertex position!
	updatePolyTipFirstP();

	// we replace the last point by the new one
	{
		QPointF pos2D = m_associatedWin->toCenteredGLCoordinates(x, y);
		CCVector3 P2D(static_cast<PointCoordinateType>(pos2D.x()),
					  static_cast<PointCoordinateType>(pos2D.y()),
					  0);

		CCVector3 *lastP = const_cast<CCVector3 *>(m_polyTipVertices->getPointPersistentPtr(1));
		*lastP = P2D;
	}

	m_polyTip->setEnabled(true);

	m_associatedWin->redraw(true, false);
}

void ccLabelDeviceTool::onItemPicked(const PickedItem &pi)
{
	if (!m_associatedWin)
	{
		assert(false);
		return;
	}

	if (!pi.entity)
	{
		// means that the mouse has been clicked but no point was found!
		return;
	}

	// if the 3D polyline doesn't exist yet, we create it
	if (!m_poly3D || !m_poly3DVertices)
	{
		m_poly3DVertices = new ccPointCloud("Vertices", static_cast<unsigned>(ReservedIDs::TRACE_POLYLINE_TOOL_POLYLINE_VERTICES));
		m_poly3DVertices->setEnabled(false);
		m_poly3DVertices->setDisplay(m_associatedWin);

		m_poly3D = new ccPolyline(m_poly3DVertices, static_cast<unsigned>(ReservedIDs::TRACE_POLYLINE_TOOL_POLYLINE));
		m_poly3D->showVertices(true);
		m_poly3D->setVertexMarkerWidth(8);
		m_poly3D->setTempColor(ccColor::red);
		m_poly3D->set2DMode(false);
		m_poly3D->addChild(m_poly3DVertices);
		m_poly3D->setWidth(2);

		ccGenericPointCloud *cloud = ccHObjectCaster::ToGenericPointCloud(pi.entity);
		m_cloud = cloud;
		if (cloud)
		{
			// copy the first clicked entity's global shift & scale
			m_poly3D->copyGlobalShiftAndScale(*cloud);
		}

		m_associatedWin->addToOwnDB(m_poly3D);
	}

	// try to add one more point
	if (!m_poly3DVertices->reserve(m_poly3DVertices->size() + 1) || !m_poly3D->reserve(m_poly3DVertices->size() + 1))
	{
		ccLog::Error("Not enough memory");
		return;
	}

	m_poly3DVertices->addPoint(pi.P3D);
	int size = std::min(unsigned int(4), m_poly3DVertices->size());
	m_poly3D->resize(size);
	if (size > 0)
		m_poly3D->addPointIndex(0, size);

	updatePolyTipFirstP();

	m_polyTip->setEnabled(false);
	updatePolyDevices();
	m_associatedWin->redraw(false, false);
}

void ccLabelDeviceTool::closePolyLine(int, int)
{
	if (!m_poly3D || (QApplication::keyboardModifiers() & Qt::ControlModifier)) // CTRL + right click = panning
	{
		return;
	}

	unsigned vertCount = m_poly3D->size();
	if (vertCount < 2)
	{
		// discard this polyline
		resetLine();
	}
	else
	{
		// hide the tip
		if (m_polyTip)
		{
			m_polyTip->setEnabled(false);
		}
		// update the GUI
		m_ui->validButton->setEnabled(true);
		m_ui->saveToolButton->setEnabled(true);
		m_ui->resetToolButton->setEnabled(true);
		m_ui->continueToolButton->setEnabled(true);
		if (m_pickingHub)
		{
			m_pickingHub->removeListener(this);
		}
		if (m_associatedWin)
		{
			m_associatedWin->setPickingMode(ccGLWindowInterface::NO_PICKING); // no more picking
			m_done = true;
			m_associatedWin->redraw(true, false);
		}
		updateTableDeviceInfo();
	}
}

void ccLabelDeviceTool::restart(bool reset)
{
	if (m_poly3D)
	{
		if (reset)
		{
			if (m_associatedWin)
			{
				// discard this polyline
				m_associatedWin->removeFromOwnDB(m_poly3D);
			}
			if (m_polyTip)
			{
				// hide the tip
				m_polyTip->setEnabled(false);
			}

			delete m_poly3D;
			m_poly3D = nullptr;
			m_poly3DVertices = nullptr;
			clearPolyDevices();
			m_ui->tableDeviceInfo->clearContents();
			m_ui->tableDeviceInfo->setRowCount(0);
		}
		else
		{
			if (m_polyTip)
			{
				// show the tip
				m_polyTip->setEnabled(true);
			}
		}
	}

	// enable picking
	if (m_pickingHub && !m_pickingHub->addListener(this, true /*, true, ccGLWindowInterface::POINT_PICKING*/))
	{
		ccLog::Error("The picking mechanism is already in use. Close the tool using it first.");
	}

	if (m_associatedWin)
	{
		m_associatedWin->redraw(false, false);
	}

	m_ui->validButton->setEnabled(false);
	m_ui->saveToolButton->setEnabled(false);
	m_ui->resetToolButton->setEnabled(false);
	m_ui->continueToolButton->setEnabled(false);
	m_done = false;
}

void ccLabelDeviceTool::exportLine()
{
	if (!m_poly3D)
	{
		return;
	}
	QString intervalName = m_ui->lineEditIntervalName->text();
	if (intervalName.isEmpty())
		intervalName = QStringLiteral( "未命名间隔");

	if (m_associatedWin)
	{
		m_associatedWin->removeFromOwnDB(m_poly3D);
		for (auto &polyDevice : m_polyDevices)
		{
			m_associatedWin->removeFromOwnDB(polyDevice);
		}
	}

	m_poly3D->enableTempColor(false);
	m_poly3D->setDisplay(m_associatedWin); // just in case
	if (MainWindow::TheInstance())
	{
		ccHObject *intervalGroup = nullptr;
		if (m_intervalNameToGroupID.contains(intervalName))
		{
			intervalGroup = m_dbRoot->find(m_intervalNameToGroupID[intervalName]);
		}
		if (!intervalGroup)
		{
			intervalGroup = new ccHObject(intervalName);
			getLabelGroup()->addChild(intervalGroup);
			m_intervalNameToGroupID[intervalName] = intervalGroup->getUniqueID();
			MainWindow::TheInstance()->addToDB(intervalGroup);
		}
		for (int i = 0; i < m_polyDevices.size(); ++i)
		{
			auto &poly3D = m_polyDevices[i];
			LabelInfo labelInfo;
			labelInfo.deviceId = m_ui->tableDeviceInfo->item(i, 0)->text();
			labelInfo.deviceName = m_ui->tableDeviceInfo->item(i, 1)->text();
			poly3D->setLabelInfo(labelInfo);
			intervalGroup->addChild(poly3D);
			MainWindow::TheInstance()->addToDB(poly3D);
		}
	}
	else
	{
		assert(false);
	}

	m_poly3D = nullptr;
	m_poly3DVertices = nullptr;
	m_polyDevices.clear();
	m_ui->tableDeviceInfo->clearContents();
	m_ui->tableDeviceInfo->setRowCount(0);

	resetLine(); // to update the GUI
}

void ccLabelDeviceTool::apply()
{

	exportLine();

	stop(true);
}

void ccLabelDeviceTool::cancel()
{
	resetLine();

	stop(false);
}

void ccLabelDeviceTool::onWidthSizeChanged(int width)
{
	if (m_poly3D)
	{
		m_poly3D->setWidth(width);
	}
	if (m_polyTip)
	{
		m_polyTip->setWidth(width);
	}

	if (m_associatedWin)
	{
		m_associatedWin->redraw(m_poly3D == nullptr, false);
	}
}

int ccLabelDeviceTool::getNextDeviceId()
{
	auto labelGroup = getLabelGroup();
	if (!labelGroup)
		return 1;
	ccHObject::Container children;
	labelGroup->filterChildren(children, true, CC_TYPES::POLY_LINE);

	int maxId = 0;
	while (!children.empty())
	{
		ccHObject *child = children.back();
		children.pop_back();
		LabelInfo labelInfo = ccHObjectCaster::ToPolyline(child)->getLabelInfo();
		if (!labelInfo.deviceId.isEmpty())
		{
			maxId = std::max(maxId, labelInfo.deviceId.toInt());
		}
	}

	for (int i = 0; i < m_ui->tableDeviceInfo->rowCount(); i++)
	{
		auto item = m_ui->tableDeviceInfo->item(i, 0);
		if (!item)
			continue; // skip empty rows
		QString deviceId = item->text();
		if (!deviceId.isEmpty())
		{
			maxId = std::max(maxId, deviceId.toInt());
		}
	}
	return maxId + 1;
}

ccHObject *ccLabelDeviceTool::getLabelGroup()
{
	ccHObject *labelGroup = nullptr;
	if (!(labelGroup = m_dbRoot->find(static_cast<unsigned>(ReservedIDs::LABEL_TOOL_LABEL_GROUP))))
	{
		labelGroup = new ccHObject(QStringLiteral("标注"), static_cast<unsigned>(ReservedIDs::LABEL_TOOL_LABEL_GROUP));
		MainWindow::TheInstance()->addToDB(labelGroup);
	}
	return labelGroup;
}

void ccLabelDeviceTool::updateIntervalGroupMap()
{
	m_intervalNameToGroupID.clear();
	auto labelGroup = getLabelGroup();
	if (!labelGroup)
		return;
	ccHObject::Container children;
	labelGroup->filterChildren(children, false, CC_TYPES::OBJECT);
	while (!children.empty())
	{
		ccHObject *child = children.back();
		children.pop_back();
		m_intervalNameToGroupID[child->getName()] = child->getUniqueID();
	}
}

std::vector<std::vector<CCVector3>> ccLabelDeviceTool::getSplitPolylines() const
{
	std::vector<std::vector<CCVector3>> result;
	unsigned pointCount = m_poly3DVertices->size();

	if (!m_poly3DVertices || pointCount < 4)
	{
		return result;
	}
	unsigned backBottomStart = 0;
	unsigned frontBottomStart = 1;
	unsigned frontBottomEnd = 2;
	unsigned frontTopEnd = 3;

	CCVector3 backBottomFirst = *(m_poly3DVertices->getPoint(backBottomStart));
	CCVector3 frontBottomFirst = *(m_poly3DVertices->getPoint(frontBottomStart));
	CCVector3 frontBottomLast = *(m_poly3DVertices->getPoint(frontBottomEnd));
	CCVector3 frontTopLast = *(m_poly3DVertices->getPoint(frontTopEnd));
	/*double findDistance = (frontBottomFirst - backBottomFirst).normd() * 0.05;
	if (std::isgreater(backBottomFirst.y ,frontBottomFirst.y))
	{
		backBottomFirst = findPoint(FindPointMode::FindFront, backBottomFirst, findDistance);
		frontBottomFirst=findPoint(FindPointMode::FindBack, frontBottomFirst,findDistance);
		frontBottomLast = findPoint(FindPointMode::FindBack, frontBottomLast, findDistance);
		frontTopLast = findPoint(FindPointMode::FindBack, frontTopLast, findDistance);
	}
	else
	{
		backBottomFirst = findPoint(FindPointMode::FindBack, backBottomFirst, findDistance);
		frontBottomFirst = findPoint(FindPointMode::FindFront, frontBottomFirst, findDistance);
		frontBottomLast = findPoint(FindPointMode::FindFront, frontBottomLast, findDistance);
		frontTopLast = findPoint(FindPointMode::FindFront, frontTopLast, findDistance);
	}*/
	
	CCVector3 totalFrontBottomEdge = frontBottomLast - frontBottomFirst;	
	CCVector3 frontTopFirst = frontTopLast - totalFrontBottomEdge;
	CCVector3 backTopFirst = frontTopFirst - (frontBottomFirst - backBottomFirst);


	std::vector<CCVector3> frontBottomPoints;
	std::vector<CCVector3> frontTopPoints;
	std::vector<CCVector3> backBottomPoints;
	std::vector<CCVector3> backTopPoints;
	frontBottomPoints.push_back(frontBottomFirst);
	frontTopPoints.push_back(frontTopFirst);
	backBottomPoints.push_back(backBottomFirst);
	backTopPoints.push_back(backTopFirst);

	std::vector<int> indexs(pointCount - 2);
	indexs[0] = frontBottomStart;
	std::iota(indexs.begin() + 1, indexs.end() - 1, 4);
	indexs[indexs.size() - 1] = frontBottomEnd;

	double totalFrontBottomEdgeLength = 0;
	for (unsigned i = 0; i < indexs.size() - 1; ++i)
	{
		const CCVector3 currentBottom = *(m_poly3DVertices->getPoint(indexs[i]));
		const CCVector3 nextBottom = *(m_poly3DVertices->getPoint(indexs[i + 1]));
		totalFrontBottomEdgeLength += (nextBottom - currentBottom).normd();
	}
	double currentFrontBottomEdgeLength = 0;
	for (unsigned i = 0; i < indexs.size() - 1; ++i)
	{
		const CCVector3 currentBottom = *(m_poly3DVertices->getPoint(indexs[i]));
		const CCVector3 nextBottom = *(m_poly3DVertices->getPoint(indexs[i + 1]));
		currentFrontBottomEdgeLength += (nextBottom - currentBottom).normd();
		double ratio = currentFrontBottomEdgeLength / totalFrontBottomEdgeLength;

		//if (std::isgreater(backBottomFirst.y ,frontBottomFirst.y))
		//{
		//	frontBottomPoints.push_back(findPoint(FindPointMode::FindBack, totalFrontBottomEdge * ratio + frontBottomFirst,findDistance));
		//	frontTopPoints.push_back(findPoint(FindPointMode::FindBack, totalFrontBottomEdge * ratio + frontTopFirst,findDistance));
		//	backBottomPoints.push_back(findPoint(FindPointMode::FindFront, totalFrontBottomEdge * ratio + backBottomFirst, findDistance));
		//	backTopPoints.push_back(findPoint(FindPointMode::FindFront, totalFrontBottomEdge * ratio + backTopFirst, findDistance));
		//}
		//else
		//{
		//	frontBottomPoints.push_back(findPoint(FindPointMode::FindFront, totalFrontBottomEdge * ratio + frontBottomFirst, findDistance));
		//	frontTopPoints.push_back(findPoint(FindPointMode::FindFront, totalFrontBottomEdge * ratio + frontTopFirst, findDistance));
		//	backBottomPoints.push_back(findPoint(FindPointMode::FindBack, totalFrontBottomEdge * ratio + backBottomFirst, findDistance));
		//	backTopPoints.push_back(findPoint(FindPointMode::FindBack, totalFrontBottomEdge * ratio + backTopFirst, findDistance));
		//}

		frontBottomPoints.push_back(totalFrontBottomEdge * ratio + frontBottomFirst);
		frontTopPoints.push_back(totalFrontBottomEdge * ratio + frontTopFirst);
		backBottomPoints.push_back(totalFrontBottomEdge * ratio + backBottomFirst);
		backTopPoints.push_back(totalFrontBottomEdge * ratio + backTopFirst);
	}
	for (int i = 0; i < frontBottomPoints.size() - 1; i++)
	{
		std::vector<CCVector3> quad;
		quad.push_back(frontBottomPoints[i]);
		quad.push_back(frontBottomPoints[i + 1]);
		quad.push_back(frontTopPoints[i + 1]);
		quad.push_back(frontTopPoints[i]);
		quad.push_back(backBottomPoints[i]);
		quad.push_back(backBottomPoints[i + 1]);
		quad.push_back(backTopPoints[i + 1]);
		quad.push_back(backTopPoints[i]);
		result.push_back(quad);
	}
	return result;
}

void ccLabelDeviceTool::updatePolyTipFirstP()
{
	int pointIndex = m_poly3DVertices->size() >= 4 ? 1 : m_poly3DVertices->size() - 1;
	const CCVector3 *P3D = m_poly3DVertices->getPoint(pointIndex);

	ccGLCameraParameters camera;
	m_associatedWin->getGLCameraParameters(camera);

	CCVector3d A2D;
	camera.project(*P3D, A2D);

	CCVector3 *firstP = const_cast<CCVector3 *>(m_polyTipVertices->getPointPersistentPtr(0));
	*firstP = CCVector3(static_cast<PointCoordinateType>(A2D.x - camera.viewport[2] / 2), // we convert A2D to centered coordinates (no need to apply high DPI scale or anything!)
						static_cast<PointCoordinateType>(A2D.y - camera.viewport[3] / 2),
						0);
}

void ccLabelDeviceTool::updatePolyDevices()
{
	clearPolyDevices();
	auto splitPolylines = getSplitPolylines();
	for (int i = 0; i < splitPolylines.size(); ++i)
	{
		auto polyline = splitPolylines[i];
		ccPointCloud *poly3DVertices = new ccPointCloud();
		poly3DVertices->setEnabled(false);
		poly3DVertices->reserve(polyline.size());
		for (auto &point : polyline)
		{
			poly3DVertices->addPoint(point);
		}

		ccPolyline *poly3D = new ccPolyline(poly3DVertices);
		poly3D->setClosed(true);
		poly3D->setTempColor(ccColor::green);
		poly3D->setDisplay(m_associatedWin);
		poly3D->addChild(poly3DVertices);
		poly3D->setWidth(2);

		poly3D->setDrawLine(true);
		poly3D->reserve(24);
		// 立方体12条边的顶点索引顺序
		// 前面4条边（逆时针）
		poly3D->addPointIndex(0); // 前面底边：0->1
		poly3D->addPointIndex(1);
		poly3D->addPointIndex(1); // 前面右边：1->2
		poly3D->addPointIndex(2);
		poly3D->addPointIndex(2); // 前面上边：2->3
		poly3D->addPointIndex(3);
		poly3D->addPointIndex(3); // 前面左边：3->0
		poly3D->addPointIndex(0);

		// 后面4条边（逆时针）
		poly3D->addPointIndex(4); // 后面底边：4->5
		poly3D->addPointIndex(5);
		poly3D->addPointIndex(5); // 后面右边：5->6
		poly3D->addPointIndex(6);
		poly3D->addPointIndex(6); // 后面上边：6->7
		poly3D->addPointIndex(7);
		poly3D->addPointIndex(7); // 后面左边：7->4
		poly3D->addPointIndex(4);

		// 连接前后面的4条边
		poly3D->addPointIndex(0); // 左后边：0->4
		poly3D->addPointIndex(4);
		poly3D->addPointIndex(1); // 右后边：1->5
		poly3D->addPointIndex(5);
		poly3D->addPointIndex(2); // 右前边：2->6
		poly3D->addPointIndex(6);
		poly3D->addPointIndex(3); // 左前边：3->7
		poly3D->addPointIndex(7);

		m_associatedWin->addToOwnDB(poly3D);
		m_polyDevices.push_back(poly3D);
	}
}

void ccLabelDeviceTool::clearPolyDevices()
{
	for (int i = 0; i < m_polyDevices.size(); ++i)
	{
		auto& polyDevice = m_polyDevices[i];
		if (m_associatedWin)
		{
			m_associatedWin->removeFromOwnDB(polyDevice);
		}
		delete polyDevice;
	}
	m_polyDevices.clear();
}

void ccLabelDeviceTool::updateTableDeviceInfo()
{
	int nextDeviceId = getNextDeviceId();
	int bIndex = m_ui->tableDeviceInfo->rowCount();
	m_ui->tableDeviceInfo->setRowCount(m_poly3DVertices->size() - 3);
	for (int i = bIndex; i < m_ui->tableDeviceInfo->rowCount(); ++i)
	{
		m_ui->tableDeviceInfo->setItem(i, 0, new QTableWidgetItem(QString::number(nextDeviceId++)));
		m_ui->tableDeviceInfo->setItem(i, 1, new QTableWidgetItem(QStringLiteral("")));
	}
}

CCVector3 ccLabelDeviceTool::findPoint(FindPointMode mode, const CCVector3 &point, double nearstDistance) const
{
	std::pair<double, CCVector3> nearestPoint(std::numeric_limits<double>::max(),point);
	bool init = false;
	int pointCount = static_cast<int>(m_cloud->size());
#ifdef CC_CORE_LIB_USES_TBB
	tbb::parallel_for(0, pointCount, [&](int i)
#else
#if defined(_OPENMP)
#pragma omp parallel for num_threads(omp_get_max_threads())
#endif
	for (int i = 0; i < pointCount; ++i)
#endif
	{
		const CCVector3 *P = m_cloud->getPoint(i);
		const double squareDist = CCVector3d(point.x - P->x, point.y - P->y, point.z - P->z).norm2d();
		if (std::isless(squareDist,nearstDistance * nearstDistance))
		{
			if (!init)
			{
				nearestPoint.first = squareDist;
				nearestPoint.second = *P;
				init = true;
				continue;
			}
			switch (mode)
			{
			case FindPointMode::FindNearest:
				if (std::isgreater(nearestPoint.first,squareDist))
				{
					nearestPoint.first = squareDist;
					nearestPoint.second = *P;
				}
				break;
			case FindPointMode::FindFront:
				if (std::isgreater(P->y,nearestPoint.second.y))
				{
					nearestPoint.first = squareDist;
					nearestPoint.second = *P;
				}
				break;
			case FindPointMode::FindBack:
				if (std::isless(P->y,nearestPoint.second.y))
				{
					nearestPoint.first = squareDist;
					nearestPoint.second = *P;
				}
				break;
			default:
				break;
			}
		} }
#ifdef CC_CORE_LIB_USES_TBB
	);
#endif
	return nearestPoint.second;
}
