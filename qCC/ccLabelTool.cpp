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

#include "ccLabelTool.h"
#include "ui_labelDlg.h"

//Local
#include "mainwindow.h"
#include "ccReservedIDs.h"

//common
#include <ccPickingHub.h>

//qCC_db
#include <ccPolyline.h>
#include <ccPointCloud.h>
#include <ccProgressDialog.h>

//System
#include <cassert>

#include "db_tree/ccDBRoot.h"


ccLabelTool::SegmentGLParams::SegmentGLParams(ccGenericGLDisplay* display, int x , int y)
{
	if (display)
	{
		display->getGLCameraParameters(params);
		QPointF pos2D = display->toCornerGLCoordinates(x, y);
		clickPos = CCVector2d(pos2D.x(), pos2D.y());
	}
}

ccLabelTool::ccLabelTool(ccPickingHub* pickingHub, QWidget* parent)
	: ccOverlayDialog(parent)
	, m_polyTip(nullptr)
	, m_polyTipVertices(nullptr)
	, m_poly3D(nullptr)
	, m_poly3DVertices(nullptr)
	, m_done(false)
	, m_pickingHub(pickingHub)
	, m_ui( new Ui::LabelDlg )
	, m_dbRoot(MainWindow::TheInstance()->db())
{
	assert(pickingHub);

	m_ui->setupUi(this);
	setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);

	connect(m_ui->saveToolButton,		&QToolButton::clicked, this, &ccLabelTool::exportLine);
	connect(m_ui->resetToolButton,		&QToolButton::clicked, this, &ccLabelTool::resetLine);
	connect(m_ui->continueToolButton,	&QToolButton::clicked, this, &ccLabelTool::continueEdition);
	connect(m_ui->validButton,			&QToolButton::clicked, this, &ccLabelTool::apply);
	connect(m_ui->cancelButton,			&QToolButton::clicked, this, &ccLabelTool::cancel);

	//add shortcuts
	addOverriddenShortcut(Qt::Key_Escape); //escape key for the "cancel" button
	addOverriddenShortcut(Qt::Key_Return); //return key for the "apply" button
	connect(this, &ccLabelTool::shortcutTriggered, this, &ccLabelTool::onShortcutTriggered);

	m_polyTipVertices = new ccPointCloud("Tip vertices", static_cast<unsigned>(ReservedIDs::TRACE_POLYLINE_TOOL_POLYLINE_TIP_VERTICES));
	m_polyTipVertices->reserve(3);
	m_polyTipVertices->addPoint(CCVector3(0, 0, 0));
	m_polyTipVertices->addPoint(CCVector3(1, 1, 1));
	m_polyTipVertices->addPoint(CCVector3(1, 1, 1));
	m_polyTipVertices->setEnabled(false);

	m_polyTip = new ccPolyline(m_polyTipVertices, static_cast<unsigned>(ReservedIDs::TRACE_POLYLINE_TOOL_POLYLINE_TIP));
	m_polyTip->setForeground(true);
	m_polyTip->setTempColor(ccColor::orange);
	m_polyTip->set2DMode(true);
	m_polyTip->reserve(3);
	m_polyTip->addPointIndex(0, 3);
	m_polyTip->setWidth(2);
	m_polyTip->addChild(m_polyTipVertices);

	m_ui->validButton->setEnabled(false);

}

ccLabelTool::~ccLabelTool()
{
	//m_polyTipVertices is already a child of m_polyTip
	if (m_polyTip)
		delete m_polyTip;
	m_polyTip = nullptr;

	//m_poly3DVertices is already a child of m_poly3D
	if (m_poly3D)
		delete m_poly3D;
	m_poly3D = nullptr;
	
	delete m_ui;
	m_ui = nullptr;
}

void ccLabelTool::onShortcutTriggered(int key)
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
		//nothing to do
		break;
	}
}

ccPolyline* ccLabelTool::polylineOverSampling(unsigned steps) const
{
	if (!m_poly3D || !m_poly3DVertices || m_segmentParams.size() != m_poly3DVertices->size())
	{
		assert(false);
		return nullptr;
	}

	if (steps <= 1)
	{
		//nothing to do
		return nullptr;
	}

	ccHObject::Container clouds;
	ccHObject::Container meshes;
	if (m_associatedWin)
	{
		m_associatedWin->getSceneDB()->filterChildren(clouds, true, CC_TYPES::POINT_CLOUD, false, m_associatedWin);
		m_associatedWin->getSceneDB()->filterChildren(meshes, true, CC_TYPES::MESH, false, m_associatedWin);
	}

	if (clouds.empty() && meshes.empty())
	{
		//no entity is currently displayed?!
		assert(false);
		return nullptr;
	}

	unsigned vertexCount = m_poly3DVertices->size();
	unsigned segmentCount = m_poly3D->size() - (m_poly3D->isClosed() ? 0 : 1);
	unsigned endSize = segmentCount * steps + (m_poly3D->isClosed() ? 0 : 1);

	ccPointCloud* newVertices = new ccPointCloud();
	ccPolyline* newPoly = new ccPolyline(newVertices);
	newPoly->addChild(newVertices);

	if (	!newVertices->reserve(endSize)
		||	!newPoly->reserve(endSize) )
	{
		ccLog::Warning("[ccLabelTool::PolylineOverSampling] Not enough memory");
		delete newPoly;
		return nullptr;
	}
	newVertices->importParametersFrom(m_poly3DVertices);
	newVertices->setName(m_poly3DVertices->getName());
	newVertices->setEnabled(m_poly3DVertices->isEnabled());
	newPoly->importParametersFrom(*m_poly3D);
	newPoly->setDisplay_recursive(m_poly3D->getDisplay());

	ccProgressDialog pDlg(true, m_associatedWin ? m_associatedWin->asWidget() : nullptr);
	pDlg.setWindowTitle("Oversampling");
	pDlg.setMethodTitle(tr("Oversampling polyline: %1 vertices").arg(endSize));
	pDlg.setRange(0, static_cast<int>(endSize));
	pDlg.show();
	QCoreApplication::processEvents();

	for (unsigned i = 0; i < segmentCount; ++i)
	{
		const CCVector3* p1 = m_poly3DVertices->getPoint(i);
		newVertices->addPoint(*p1);


		unsigned i2 = (i + 1) % vertexCount;
		CCVector2d clickPos1/* = m_segmentParams[i].clickPos*/;
		{
			//we actually retro-project the 3D point in the second vertex camera frame so as to get a proper behavior
			CCVector3d P2D;
			m_segmentParams[i2].params.project(*p1, P2D);
			clickPos1 = CCVector2d(P2D.x, P2D.y);
		}

		CCVector2d v = m_segmentParams[i2].clickPos - clickPos1;
		v /= steps;

		for (unsigned j = 1; j < steps; j++)
		{
			CCVector2d vj = clickPos1 + v * j;

			CCVector3 nearestPoint;
			double nearestElementSquareDist = -1.0;

			//for each cloud
			for (size_t c = 0; c < clouds.size(); ++c)
			{
				ccGenericPointCloud* cloud = static_cast<ccGenericPointCloud*>(clouds[c]);
				if (!cloud->isDisplayedIn(m_associatedWin))
				{
					continue;
				}
				
				int nearestPointIndex = -1;
				double nearestSquareDist = 0;
				if (cloud->pointPicking(vj,
										m_segmentParams[i2].params,
										nearestPointIndex,
										nearestSquareDist))
				{
					if (nearestElementSquareDist < 0 || nearestSquareDist < nearestElementSquareDist)
					{
						nearestElementSquareDist = nearestSquareDist;
						nearestPoint = *cloud->getPoint(nearestPointIndex);
					}
				}
			}

			//for each mesh
			for (size_t m = 0; m < meshes.size(); ++m)
			{
				ccGenericMesh* mesh = static_cast<ccGenericMesh*>(meshes[m]);
				if (!mesh->isDisplayedIn(m_associatedWin))
				{
					continue;
				}

				int nearestTriIndex = -1;
				double nearestSquareDist = 0;
				CCVector3d _nearestPoint;

				if (mesh->trianglePicking(	vj,
											m_segmentParams[i2].params,
											nearestTriIndex,
											nearestSquareDist,
											_nearestPoint))
				{
					if (nearestElementSquareDist < 0 || nearestSquareDist < nearestElementSquareDist)
					{
						nearestElementSquareDist = nearestSquareDist;
						nearestPoint = _nearestPoint.toPC();
					}
				}
			}

			if (nearestElementSquareDist >= 0)
			{
				newVertices->addPoint(nearestPoint);
			}

			if (pDlg.wasCanceled())
			{
				steps = 0; //quick finish ;)
				break;
			}
			pDlg.setValue(pDlg.value() + 1);
		}
	}

	//add last point
	if (!m_poly3D->isClosed())
	{
		newVertices->addPoint(*m_poly3DVertices->getPoint(vertexCount - 1));
	}

	newVertices->shrinkToFit();
	newPoly->addPointIndex(0, newVertices->size());

	return newPoly;
}

bool ccLabelTool::linkWith(ccGLWindowInterface* win)
{
	assert(m_polyTip);
	assert(!m_poly3D || !win);

	ccGLWindowInterface* oldWin = m_associatedWin;

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
		connect(m_associatedWin->signalEmitter(), &ccGLWindowSignalEmitter::rightButtonClicked,	this, &ccLabelTool::closePolyLine);
		connect(m_associatedWin->signalEmitter(), &ccGLWindowSignalEmitter::mouseMoved,			this, &ccLabelTool::updatePolyLineTip);
	}

	return true;
}

static int s_defaultPickingRadius = 1;
static int s_overSamplingCount = 1;
bool ccLabelTool::start()
{
	assert(m_polyTip);
	assert(!m_poly3D);

	if (!m_associatedWin)
	{
		ccLog::Warning("[Trace Polyline Tool] No associated window!");
		return false;
	}
	//初始化
	m_ui->lineEditDeviceId->setText(QString::number(getNextDeviceId()));
	updateIntervalGroupMap();

	m_associatedWin->setUnclosable(true);
	m_associatedWin->addToOwnDB(m_polyTip);
	if (m_pickingHub)
	{
		m_pickingHub->removeListener(this);
	}
	m_associatedWin->setPickingMode(ccGLWindowInterface::NO_PICKING);
	m_associatedWin->setInteractionMode(	ccGLWindowInterface::MODE_TRANSFORM_CAMERA
										|	ccGLWindowInterface::INTERACT_SIG_RB_CLICKED
										|	ccGLWindowInterface::INTERACT_CTRL_PAN
										|	ccGLWindowInterface::INTERACT_SIG_MOUSE_MOVED);
	m_associatedWin->setWindowCursor(Qt::CrossCursor);

	resetLine(); //to reset the GUI

	return ccOverlayDialog::start();
}

void ccLabelTool::stop(bool accepted)
{
	assert(m_polyTip);

	if (m_pickingHub)
	{
		m_pickingHub->removeListener(this);
	}

	if (m_associatedWin)
	{
		m_associatedWin->displayNewMessage(	"Polyline tracing [OFF]",
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

void ccLabelTool::updatePolyLineTip(int x, int y, Qt::MouseButtons buttons)
{
	if (!m_associatedWin)
	{
		assert(false);
		return;
	}
	
	if (buttons != Qt::NoButton)
	{
		//nothing to do (just hide the tip)
		if (m_polyTip->isEnabled())
		{
			m_polyTip->setEnabled(false);
			m_associatedWin->redraw(true, false);
		}
		return;
	}

	if (!m_poly3DVertices || m_poly3DVertices->size() == 0)
	{
		//there should be at least one point already picked!
		return;
	}

	if (m_done)
	{
		// when it is done do nothing
		return;
	}

	assert(m_polyTip && m_polyTipVertices && m_polyTipVertices->size() == 3);
	//just in case (e.g. if the view has been rotated or zoomed)
	//we also update the first vertex position!
	{
		const CCVector3* P3D = m_poly3DVertices->getPoint(m_poly3DVertices->size() - 1);

		ccGLCameraParameters camera;
		m_associatedWin->getGLCameraParameters(camera);

		CCVector3d A2D;
		camera.project(*P3D, A2D);

		CCVector3* firstP = const_cast<CCVector3*>(m_polyTipVertices->getPointPersistentPtr(0));
		*firstP = CCVector3(static_cast<PointCoordinateType>(A2D.x - camera.viewport[2] / 2), //we convert A2D to centered coordinates (no need to apply high DPI scale or anything!)
			static_cast<PointCoordinateType>(A2D.y - camera.viewport[3] / 2),
			0);

	}


	//we replace the last point by the new one
	{
		QPointF pos2D = m_associatedWin->toCenteredGLCoordinates(x, y);
		CCVector3 P2D(	static_cast<PointCoordinateType>(pos2D.x()),
						static_cast<PointCoordinateType>(pos2D.y()),
						0);

		CCVector3* middleP = const_cast<CCVector3*>(m_polyTipVertices->getPointPersistentPtr(1));
		*middleP = P2D;
	}

	{
		const CCVector3* P3D = m_poly3DVertices->getPoint(0);

		ccGLCameraParameters camera;
		m_associatedWin->getGLCameraParameters(camera);

		CCVector3d A2D;
		camera.project(*P3D, A2D);

		CCVector3* lastP = const_cast<CCVector3*>(m_polyTipVertices->getPointPersistentPtr(2));
		*lastP = CCVector3(static_cast<PointCoordinateType>(A2D.x - camera.viewport[2] / 2), //we convert A2D to centered coordinates (no need to apply high DPI scale or anything!)
			static_cast<PointCoordinateType>(A2D.y - camera.viewport[3] / 2),
			0);
	}


	m_polyTip->setEnabled(true);

	m_associatedWin->redraw(true, false);
}


void ccLabelTool::onItemPicked(const PickedItem& pi)
{
	if (!m_associatedWin)
	{
		assert(false);
		return;
	}

	if (!pi.entity)
	{
		//means that the mouse has been clicked but no point was found!
		return;
	}

	//if the 3D polyline doesn't exist yet, we create it
	if (!m_poly3D || !m_poly3DVertices)
	{
		m_poly3DVertices = new ccPointCloud("Vertices", static_cast<unsigned>(ReservedIDs::TRACE_POLYLINE_TOOL_POLYLINE_VERTICES));
		m_poly3DVertices->setEnabled(false);
		m_poly3DVertices->setDisplay(m_associatedWin);

		m_poly3D = new ccPolyline(m_poly3DVertices, static_cast<unsigned>(ReservedIDs::TRACE_POLYLINE_TOOL_POLYLINE));
		m_poly3D->setClosed(true);
		m_poly3D->setTempColor(ccColor::green);
		m_poly3D->set2DMode(false);
		m_poly3D->addChild(m_poly3DVertices);
		m_poly3D->setWidth(2); 

		ccGenericPointCloud* cloud = ccHObjectCaster::ToGenericPointCloud(pi.entity);
		if (cloud)
		{
			//copy the first clicked entity's global shift & scale
			m_poly3D->copyGlobalShiftAndScale(*cloud);
		}

		m_segmentParams.resize(0); //just in case

		m_associatedWin->addToOwnDB(m_poly3D);
	}

	//try to add one more point
	if (	!m_poly3DVertices->reserve(m_poly3DVertices->size() + 1)
		||	!m_poly3D->reserve(m_poly3DVertices->size() + 1))
	{
		ccLog::Error("Not enough memory");
		return;
	}

	try
	{
		m_segmentParams.reserve(m_segmentParams.size() + 1);
	}
	catch (const std::bad_alloc&)
	{
		ccLog::Error("Not enough memory");
		return;
	}

	m_poly3DVertices->addPoint(pi.P3D);
	m_poly3D->addPointIndex(m_poly3DVertices->size() - 1);
	m_segmentParams.emplace_back(m_associatedWin, pi.clickPoint.x(), pi.clickPoint.y());
	if (m_poly3DVertices->size() == 1)
	{
		const CCVector3* P3D = m_poly3DVertices->getPoint(0);

		ccGLCameraParameters camera;
		m_associatedWin->getGLCameraParameters(camera);

		CCVector3d A2D;
		camera.project(*P3D, A2D);

		CCVector3* lastP = const_cast<CCVector3*>(m_polyTipVertices->getPointPersistentPtr(2));
		*lastP = CCVector3(static_cast<PointCoordinateType>(A2D.x - camera.viewport[2] / 2), //we convert A2D to centered coordinates (no need to apply high DPI scale or anything!)
			static_cast<PointCoordinateType>(A2D.y - camera.viewport[3] / 2),
			0);
	}
	//we replace the first point of the tip by this new point
	{
		QPointF pos2D = m_associatedWin->toCenteredGLCoordinates(pi.clickPoint.x(), pi.clickPoint.y());
		CCVector3 P2D(	static_cast<PointCoordinateType>(pos2D.x()),
						static_cast<PointCoordinateType>(pos2D.y()),
						0);

		CCVector3* firstTipPoint = const_cast<CCVector3*>(m_polyTipVertices->getPointPersistentPtr(0));
		*firstTipPoint = P2D;
		m_polyTip->setEnabled(false); //don't need to display it for now
	}

	m_associatedWin->redraw(false, false);
}

void ccLabelTool::closePolyLine(int, int)
{
	if (!m_poly3D || (QApplication::keyboardModifiers() & Qt::ControlModifier)) //CTRL + right click = panning
	{
		return;
	}

	unsigned vertCount = m_poly3D->size();
	if (vertCount < 2)
	{
		//discard this polyline
		resetLine();
	}
	else
	{
		//hide the tip
		if (m_polyTip)
		{
			m_polyTip->setEnabled(false);
		}
		//update the GUI
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
			m_associatedWin->setPickingMode(ccGLWindowInterface::NO_PICKING); //no more picking
			m_done = true;
			m_associatedWin->redraw(true, false);
		}
	}
}

void ccLabelTool::restart(bool reset)
{
	if (m_poly3D)
	{
		if (reset)
		{
			if (m_associatedWin)
			{
				//discard this polyline
				m_associatedWin->removeFromOwnDB(m_poly3D);
			}
			if (m_polyTip)
			{
				//hide the tip
				m_polyTip->setEnabled(false);
			}

			delete m_poly3D;
			m_segmentParams.resize(0);
			//delete m_poly3DVertices;
			m_poly3D = nullptr;
			m_poly3DVertices = nullptr;
		}
		else
		{
			if (m_polyTip)
			{
				//show the tip
				m_polyTip->setEnabled(true);
			}
		}
	}

	//enable picking
	if (m_pickingHub && !m_pickingHub->addListener(this, true/*, true, ccGLWindowInterface::POINT_PICKING*/))
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

void ccLabelTool::exportLine()
{
	if (!m_poly3D)
	{
		return;
	}
	QString intervalName=m_ui->lineEditIntervalName->text();
	QString deviceName=m_ui->lineEditDeviceName->text();
	QString id=m_ui->lineEditDeviceId->text();

	if (m_associatedWin)
	{
		m_associatedWin->removeFromOwnDB(m_poly3D);
	}

	
	m_poly3D->setUniqueID(ccObject::GetNextUniqueID());
	m_poly3DVertices->setUniqueID(ccObject::GetNextUniqueID());
	m_poly3D->setLabelInfoType(LabelInfoType::Device);
	m_poly3D->setDeviceInfo(DeviceLabelInfo{id,deviceName});

	m_poly3D->enableTempColor(false);
	m_poly3D->setDisplay(m_associatedWin); //just in case
	if (MainWindow::TheInstance())
	{
		ccHObject* intervalGroup=nullptr;
		if(m_intervalNameToGroupID.contains(intervalName))
		{
			intervalGroup = m_dbRoot->find(m_intervalNameToGroupID[intervalName]);
		}
		if(!intervalGroup)
		{
			intervalGroup = new ccHObject(intervalName);
			getLabelGroup()->addChild(intervalGroup);
			m_intervalNameToGroupID[intervalName] = intervalGroup->getUniqueID();
			MainWindow::TheInstance()->addToDB(intervalGroup);
		}
		intervalGroup->addChild(m_poly3D);
		MainWindow::TheInstance()->addToDB(m_poly3D);
	}
	else
	{
		assert(false);
	}

	m_poly3D = nullptr;
	m_segmentParams.resize(0);
	m_poly3DVertices = nullptr;

	resetLine(); //to update the GUI
}

void ccLabelTool::apply()
{
	if(m_ui->lineEditIntervalName->text().isEmpty()||m_ui->lineEditDeviceName->text().isEmpty()||m_ui->lineEditDeviceId->text().isEmpty())
	{
		ccLog::Error(QStringLiteral("间隔名称、设备名称、编号不能为空"));
		return;
	}

	exportLine();

	stop(true);
}

void ccLabelTool::cancel()
{
	resetLine();

	stop(false);
}

void ccLabelTool::onWidthSizeChanged(int width)
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

int ccLabelTool::getNextDeviceId()
{
	auto labelGroup=getLabelGroup();
	if(!labelGroup)
		return 1;
	ccHObject::Container children;
	labelGroup->filterChildren(children,true,CC_TYPES::POLY_LINE);

	int maxId=0;
	while(!children.empty())
	{
		ccHObject* child=children.back();
		children.pop_back();
		DeviceLabelInfo labelInfo=child->getDeviceInfo();
		if(!labelInfo.deviceId.isEmpty())
		{
			int tmpId=labelInfo.deviceId.toInt();
			if(tmpId>maxId)
			{
				maxId=tmpId;
			}
		}
	}
	return maxId+1;
}


ccHObject* ccLabelTool::getLabelGroup()
{
	ccHObject* labelGroup=nullptr;
	if(!(labelGroup=m_dbRoot->find(static_cast<unsigned>(ReservedIDs::LABEL_TOOL_LABEL_GROUP))))
	{
		labelGroup = new ccHObject(QStringLiteral("标注"), static_cast<unsigned>(ReservedIDs::LABEL_TOOL_LABEL_GROUP));
		MainWindow::TheInstance()->addToDB(labelGroup);
	}
	return labelGroup;
}


void ccLabelTool::updateIntervalGroupMap()
{
	m_intervalNameToGroupID.clear();
	auto labelGroup=getLabelGroup();
	if(!labelGroup)
		return;
	ccHObject::Container children;
	labelGroup->filterChildren(children,true,CC_TYPES::OBJECT);
	while(!children.empty())
	{
		ccHObject* child=children.back();
		children.pop_back();
		m_intervalNameToGroupID[child->getName()]=child->getUniqueID();
	}
}
