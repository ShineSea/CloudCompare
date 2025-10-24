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

#include "ccLabelAreaTool.h"
#include "ui_labelAreaDlg.h"

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
#include <QShortcut>
#include "db_tree/ccDBRoot.h"

#include<quuid.h>

QString ccLabelAreaTool::m_lastFactoryId;
QString ccLabelAreaTool::m_lastAreaId;
QString ccLabelAreaTool::m_lastIntervalId;
ccLabelAreaTool::SegmentGLParams::SegmentGLParams(ccGenericGLDisplay* display, int x , int y)
{
	if (display)
	{
		display->getGLCameraParameters(params);
		QPointF pos2D = display->toCornerGLCoordinates(x, y);
		clickPos = CCVector2d(pos2D.x(), pos2D.y());
	}
}

ccLabelAreaTool::ccLabelAreaTool(ccPickingHub* pickingHub, QWidget* parent)
	: ccOverlayDialog(parent)
	, m_polyTip(nullptr)
	, m_polyTipVertices(nullptr)
	, m_poly3D(nullptr)
	, m_poly3DVertices(nullptr)
	, m_done(false)
	, m_pickingHub(pickingHub)
	, m_ui( new Ui::LabelAreaDlg )
	, m_dbRoot(MainWindow::TheInstance()->db())
{
	assert(pickingHub);

	m_ui->setupUi(this);
	setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);

	connect(m_ui->saveToolButton,		&QToolButton::clicked, this, &ccLabelAreaTool::exportLine);
	connect(m_ui->resetToolButton,		&QToolButton::clicked, this, &ccLabelAreaTool::resetLine);
	connect(m_ui->validButton,			&QToolButton::clicked, this, &ccLabelAreaTool::apply);
	connect(m_ui->cancelButton,			&QToolButton::clicked, this, &ccLabelAreaTool::cancel);

	//add shortcuts
	addOverriddenShortcut(Qt::Key_Escape); //escape key for the "cancel" button
	addOverriddenShortcut(Qt::Key_Return); //return key for the "apply" button
	connect(this, &ccLabelAreaTool::shortcutTriggered, this, &ccLabelAreaTool::onShortcutTriggered);

	QShortcut* undoShortcut = new QShortcut(QKeySequence::Undo, this);
	undoShortcut->setContext(Qt::ApplicationShortcut);
	connect(undoShortcut, &QShortcut::activated, this, &ccLabelAreaTool::undo);

	m_polyTipVertices = new ccPointCloud("Tip vertices", static_cast<unsigned>(ReservedIDs::TRACE_POLYLINE_TOOL_POLYLINE_TIP_VERTICES));
	m_polyTipVertices->reserve(2);
	m_polyTipVertices->addPoint(CCVector3(0, 0, 0));
	m_polyTipVertices->addPoint(CCVector3(1, 1, 0));
	m_polyTipVertices->setEnabled(false);

	m_polyTip = new ccPolyline(m_polyTipVertices, static_cast<unsigned>(ReservedIDs::TRACE_POLYLINE_TOOL_POLYLINE_TIP));
	m_polyTip->setForeground(true);
	m_polyTip->setTempColor(ccColor::orange);
	m_polyTip->set2DMode(true);
	m_polyTip->reserve(2);
	m_polyTip->addPointIndex(0, 2);
	m_polyTip->setWidth(2);
	m_polyTip->addChild(m_polyTipVertices);

	m_ui->validButton->setEnabled(false);

	connect(m_ui->comboBoxFactory, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int index) {
		m_lastFactoryId = m_ui->comboBoxFactory->itemData(index).value<FactoryLabelInfo>().factoryId;
		updateAreaComboBox();
		});
	connect(m_ui->comboBoxArea, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int index) {
		m_lastAreaId = m_ui->comboBoxArea->itemData(index).value<AreaLabelInfo>().areaId;
		updateIntervalComboBox();
		});
	connect(m_ui->comboBoxInterval, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int index) {
		m_lastIntervalId = m_ui->comboBoxInterval->itemData(index).value<IntervalLabelInfo>().intervalId;
		});

}

ccLabelAreaTool::~ccLabelAreaTool()
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

void ccLabelAreaTool::onShortcutTriggered(int key)
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


bool ccLabelAreaTool::linkWith(ccGLWindowInterface* win)
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
		connect(m_associatedWin->signalEmitter(), &ccGLWindowSignalEmitter::rightButtonClicked,	this, &ccLabelAreaTool::closePolyLine);
		connect(m_associatedWin->signalEmitter(), &ccGLWindowSignalEmitter::mouseMoved,			this, &ccLabelAreaTool::updatePolyLineTip);
	}

	return true;
}

static int s_defaultPickingRadius = 1;
static int s_overSamplingCount = 1;
bool ccLabelAreaTool::start()
{
	assert(m_polyTip);
	assert(!m_poly3D);
	updateLabelInfos();
	updateFactoryComboBox();

	if (!m_associatedWin)
	{
		ccLog::Warning("[Trace Polyline Tool] No associated window!");
		return false;
	}

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

void ccLabelAreaTool::stop(bool accepted)
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

void ccLabelAreaTool::updatePolyLineTip(int x, int y, Qt::MouseButtons buttons)
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

	assert(m_polyTip && m_polyTipVertices && m_polyTipVertices->size() == 2);

	updatePolyTipFirstP();

	// 更新提示线的中点：鼠标当前2D位置（中心化）
	{
		QPointF pos2D = m_associatedWin->toCenteredGLCoordinates(x, y);
		CCVector3* middleTipPoint = const_cast<CCVector3*>(m_polyTipVertices->getPointPersistentPtr(1));
		*middleTipPoint = CCVector3(
			static_cast<PointCoordinateType>(pos2D.x()),
			static_cast<PointCoordinateType>(pos2D.y()),
			0
		);
	}

	m_polyTip->setEnabled(true);

	m_associatedWin->redraw(true, false);
}

void ccLabelAreaTool::updatePolyTipFirstP()
{
	// 将3D点投影到屏幕坐标（中心为原点）
	auto projectToScreen = [this](const CCVector3& p3D) -> CCVector3
		{
			ccGLCameraParameters camera;
			m_associatedWin->getGLCameraParameters(camera);

			CCVector3d projected2D;
			camera.project(p3D, projected2D);

			// 转换为以窗口中心为原点的坐标
			return CCVector3(
				static_cast<PointCoordinateType>(projected2D.x - camera.viewport[2] / 2),
				static_cast<PointCoordinateType>(projected2D.y - camera.viewport[3] / 2),
				0
			);
		};

	// 更新提示线的第一个端点：最后一个多边形顶点投影到屏幕
	{
		const CCVector3& lastVertex3D = *m_poly3DVertices->getPoint(m_poly3DVertices->size() - 1);
		CCVector3* firstTipPoint = const_cast<CCVector3*>(m_polyTipVertices->getPointPersistentPtr(0));
		*firstTipPoint = projectToScreen(lastVertex3D);
	}
}


void ccLabelAreaTool::onItemPicked(const PickedItem& pi)
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
	if (m_poly3DVertices->size() ==3)
	{
		// 自动添加最后一个点，闭合多边形
		if (!m_poly3DVertices->reserve(m_poly3DVertices->size() + 1)
			|| !m_poly3D->reserve(m_poly3DVertices->size() + 1))
		{
			ccLog::Error("Not enough memory");
			return;
		}

		// 计算第4个点 D = A + C - B
		CCVector3 lastPoint = *m_poly3DVertices->getPointPersistentPtr(0)
			+ *m_poly3DVertices->getPointPersistentPtr(2)
			- *m_poly3DVertices->getPointPersistentPtr(1);

		m_poly3DVertices->addPoint(lastPoint);
		m_poly3D->addPointIndex(m_poly3DVertices->size() - 1);

		// 压平Z轴
		float minZ = std::numeric_limits<float>::max();
		for (unsigned i = 0; i < m_poly3DVertices->size(); ++i)
		{
			const CCVector3* point = m_poly3DVertices->getPointPersistentPtr(i);
			minZ = std::min(minZ, point->z);
		}
		for (int i = 0; i < static_cast<int>(m_poly3DVertices->size()); ++i)
		{
			CCVector3* point = const_cast<CCVector3*>(m_poly3DVertices->getPointPersistentPtr(i));
			point->z = minZ;
		}

		// =======================================================
		// 🔹 检查夹角并自动调整成矩形
		// =======================================================

		if (m_poly3DVertices->size() >= 4)
		{
			CCVector3 A = *m_poly3DVertices->getPointPersistentPtr(0);
			CCVector3 B = *m_poly3DVertices->getPointPersistentPtr(1);
			CCVector3 D = *m_poly3DVertices->getPointPersistentPtr(3);

			CCVector3 AB = B - A;
			CCVector3 AD = D - A;

			float abLen = AB.norm();
			float adLen = AD.norm();
			if (abLen > 1e-6f && adLen > 1e-6f)
			{
				// 计算夹角
				float cosAngle = CCVector3::vdot(AB.u, AD.u) / (abLen * adLen);
				cosAngle = std::clamp(cosAngle, -1.0f, 1.0f);
				float angleDeg = acosf(cosAngle) * 180.0f / static_cast<float>(M_PI);

				// 如果夹角接近直角，则正交化
				if (angleDeg > 80.0f && angleDeg < 100.0f)
				{
					ccLog::Print(QString("[AutoRectify] Angle = %1°, correcting to rectangle...").arg(angleDeg));

					// 得到法向量和平面正交方向
					CCVector3 normal = AB.cross(AD);
					if (normal.norm2() > 1e-10f)
					{
						normal.normalize();
						// 新的正交方向：垂直于AB
						CCVector3 newAD = normal.cross(AB);
						newAD.normalize();
						newAD *= adLen;

						// 更新 D, C 点
						CCVector3 D_new = A + newAD;
						CCVector3 C_new = D_new + (B - A);

						// 写回顶点
						*const_cast<CCVector3*>(m_poly3DVertices->getPointPersistentPtr(2)) = C_new;
						*const_cast<CCVector3*>(m_poly3DVertices->getPointPersistentPtr(3)) = D_new;
					}
				}
			}
		}
		closePolyLine();
		m_associatedWin->redraw(false, false);
		return;
	}
	m_segmentParams.emplace_back(m_associatedWin, pi.clickPoint.x(), pi.clickPoint.y());
	//we replace the first point of the tip by this new point
	{
		//获取点击的平面点
		QPointF pos2D = m_associatedWin->toCenteredGLCoordinates(pi.clickPoint.x(), pi.clickPoint.y());
		CCVector3 P2D(static_cast<PointCoordinateType>(pos2D.x()),
			static_cast<PointCoordinateType>(pos2D.y()),
			0);
		CCVector3* firstTipPoint = const_cast<CCVector3*>(m_polyTipVertices->getPointPersistentPtr(0));
		*firstTipPoint = P2D;
		m_polyTip->setEnabled(false); //don't need to display it for now
	}

	m_associatedWin->redraw(false, false);
}

void ccLabelAreaTool::closePolyLine(int, int)
{
	if (!m_poly3D || (QApplication::keyboardModifiers() & Qt::ControlModifier)) //CTRL + right click = panning
	{
		return;
	}

	unsigned vertCount = m_poly3D->size();
	if (vertCount <4)
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

void ccLabelAreaTool::restart(bool reset)
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
	m_done = false;
}

void ccLabelAreaTool::updateLabelInfos()
{
	m_factoryInfoList.clear();
	m_areaInfoMap.clear();
	m_intervalInfoMap.clear();
	m_intervalMap.clear();
	MainWindow* mainWindow = MainWindow::TheInstance();
	ccDBRoot* db = mainWindow->db();
	ccHObject* labelGroup = db->getLabelGroup();
	ccHObject::Container labelChildren;
	labelGroup->filterChildren(labelChildren, false, CC_TYPES::HIERARCHY_OBJECT);
	while (!labelChildren.empty())
	{
		ccHObject* child = labelChildren.back();
		labelChildren.pop_back();
		if (child->getLabelInfoType() != LabelInfoType::Factory)
		{
			continue;
		}
		FactoryLabelInfo factoryInfo = child->getFactoryInfo();
		m_factoryInfoList.append(factoryInfo);
		ccHObject::Container factoryChildren;
		child->filterChildren(factoryChildren, false, CC_TYPES::HIERARCHY_OBJECT);
		while (!factoryChildren.empty())
		{
			ccHObject* child = factoryChildren.back();
			factoryChildren.pop_back();
			if (child->getLabelInfoType() != LabelInfoType::Area)
			{
				continue;
			}
			AreaLabelInfo areaInfo = child->getAreaInfo();
			m_areaInfoMap[factoryInfo.factoryId].append(areaInfo);
			ccHObject::Container areaChildren;
			child->filterChildren(areaChildren, false, CC_TYPES::HIERARCHY_OBJECT);
			while (!areaChildren.empty())
			{
				ccHObject* child = areaChildren.back();
				areaChildren.pop_back();
				if (child->getLabelInfoType() != LabelInfoType::Interval)
				{
					continue;
				}
				IntervalLabelInfo intervalInfo = child->getIntervalInfo();
				m_intervalInfoMap[areaInfo.areaId].append(intervalInfo);
				m_intervalMap.insert(intervalInfo.intervalId, child);
			}

		}
	}
}

void ccLabelAreaTool::updateFactoryComboBox()
{
	m_ui->comboBoxFactory->blockSignals(true);
	m_ui->comboBoxFactory->clear();
	for (const auto& info : m_factoryInfoList)
		m_ui->comboBoxFactory->addItem(info.factoryName, QVariant::fromValue(info));
	if (!m_lastFactoryId.isEmpty())
	{
		for (int i = 0; i < m_ui->comboBoxFactory->count(); ++i)
		{
			if (m_ui->comboBoxFactory->itemData(i).value<FactoryLabelInfo>().factoryId == m_lastFactoryId)
			{
				m_ui->comboBoxFactory->setCurrentIndex(i);
				break;
			}
		}
	}
	m_lastFactoryId = m_ui->comboBoxFactory->currentData().value<FactoryLabelInfo>().factoryId;
	m_ui->comboBoxFactory->blockSignals(false);
	updateAreaComboBox();
}

void ccLabelAreaTool::updateAreaComboBox()
{
	m_ui->comboBoxArea->blockSignals(true);
	m_ui->comboBoxArea->clear();
	for (const auto& info : m_areaInfoMap[m_lastFactoryId])
		m_ui->comboBoxArea->addItem(info.areaName, QVariant::fromValue(info));
	if (!m_lastAreaId.isEmpty())
	{
		for (int i = 0; i < m_ui->comboBoxArea->count(); ++i)
		{
			if (m_ui->comboBoxArea->itemData(i).value<AreaLabelInfo>().areaId == m_lastAreaId)
			{
				m_ui->comboBoxArea->setCurrentIndex(i);
				break;
			}
		}
	}
	m_lastAreaId = m_ui->comboBoxArea->currentData().value<AreaLabelInfo>().areaId;
	m_ui->comboBoxArea->blockSignals(false);
	updateIntervalComboBox();
}

void ccLabelAreaTool::updateIntervalComboBox()
{
	m_ui->comboBoxInterval->blockSignals(true);
	m_ui->comboBoxInterval->clear();
	for (const auto& info : m_intervalInfoMap[m_lastAreaId])
		m_ui->comboBoxInterval->addItem(info.intervalName, QVariant::fromValue(info));
	if (!m_lastIntervalId.isEmpty())
	{
		for (int i = 0; i < m_ui->comboBoxInterval->count(); ++i)
		{
			if (m_ui->comboBoxInterval->itemData(i).value<IntervalLabelInfo>().intervalId == m_lastIntervalId)
			{
				m_ui->comboBoxInterval->setCurrentIndex(i);
				break;
			}
		}
	}
	m_lastIntervalId = m_ui->comboBoxInterval->currentData().value<IntervalLabelInfo>().intervalId;
	m_ui->comboBoxInterval->blockSignals(false);
}

void ccLabelAreaTool::exportLine()
{
	if (!m_poly3D)
	{
		return;
	}
	if (m_associatedWin)
	{
		m_associatedWin->removeFromOwnDB(m_poly3D);
	}
	if (MainWindow::TheInstance())
	{
		ccHObject* intervalGroup = m_intervalMap[m_lastIntervalId];
		if (!intervalGroup)
		{
			delete m_poly3D;
			m_poly3D = nullptr;
			m_poly3DVertices = nullptr;
			return;
		}
		m_poly3D->setUniqueID(ccObject::GetNextUniqueID());
		m_poly3DVertices->setUniqueID(ccObject::GetNextUniqueID());
		m_poly3D->setLabelInfoType(LabelInfoType::Station);
		m_poly3D->enableTempColor(false);
		m_poly3D->setDisplay(m_associatedWin); 
		StationLabelInfo labelInfo;
		labelInfo.stationId= QUuid::createUuid().toString().remove("{").remove("}").remove('-');
		labelInfo.stationName = kStationDefaultName;
		m_poly3D->setStationInfo(labelInfo);
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

void ccLabelAreaTool::undo()
{
	if (m_done)
		return;
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
		for (const auto& point : points)
		{
			m_poly3DVertices->addPoint(point);
		}
	}
	if (m_poly3D)
	{
		m_poly3D->resize(m_poly3DVertices->size());
		m_poly3D->addPointIndex(0, m_poly3DVertices->size());
	}
	if (m_poly3DVertices->size() >= 1)
	{
		updatePolyTipFirstP();
	}
	m_polyTip->setEnabled(true);
	m_poly3D->setEnabled(true);
	m_associatedWin->redraw(false, false);
}

void ccLabelAreaTool::apply()
{
	exportLine();

	stop(true);
}

void ccLabelAreaTool::cancel()
{
	resetLine();

	stop(false);
}

void ccLabelAreaTool::onWidthSizeChanged(int width)
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

