#include "ccLabelPathDlg.h"

//Qt
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>

//CCCoreLib
#include <CCConst.h>

//qCC_db
#include <cc2DLabel.h>
#include <ccLog.h>
#include <ccPointCloud.h>
#include <ccPolyline.h>

//qCC_io
#include <AsciiFilter.h>

//qCC_gl
#include <ccGLWindowInterface.h>

//local
#include "mainwindow.h"
#include "db_tree/ccDBRoot.h"

//system
#include <cassert>

//semi persistent settings
static bool s_showGlobalCoordsCheckBoxChecked = false;
static const char s_pickedPointContainerName[] = "路径标注";
static const char s_defaultLabelBaseName[] = "Point #";

ccLabelPathDlg::ccLabelPathDlg(ccPickingHub* pickingHub, QWidget* parent)
    : ccPointPickingGenericInterface(pickingHub, parent)
    , Ui::LabelPathDlg()
    , m_associatedEntity(nullptr)
    , m_lastPreviousID(0)
	,m_lastPathID(0)
    , m_orderedLabelsContainer(nullptr)
{
    setupUi(this);

	tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	showGlobalCoordsCheckBox->setChecked(s_showGlobalCoordsCheckBoxChecked);

	connect(cancelToolButton,		&QAbstractButton::clicked,	this,				&ccLabelPathDlg::cancelAndExit);
	connect(revertToolButton,		&QAbstractButton::clicked,	this,				&ccLabelPathDlg::removeLastEntry);
	connect(validToolButton,		&QAbstractButton::clicked,	this,				&ccLabelPathDlg::applyAndExit);

	connect(markerSizeSpinBox,	qOverload<int>(&QSpinBox::valueChanged),	this,	&ccLabelPathDlg::markerSizeChanged);
	
	connect(showGlobalCoordsCheckBox, &QAbstractButton::clicked, this, &ccLabelPathDlg::updateList);

	updateList();
}

unsigned ccLabelPathDlg::getPickedPoints(std::vector<cc2DLabel*>& pickedPoints)
{
	pickedPoints.clear();

	if (m_orderedLabelsContainer)
	{
		//get all labels
		ccHObject::Container labels;
		unsigned count = m_orderedLabelsContainer->filterChildren(labels, false, CC_TYPES::LABEL_2D);

		try
		{
			pickedPoints.reserve(count);
		}
		catch (const std::bad_alloc&)
		{
			ccLog::Error("Not enough memory!");
			return 0;
		}
		for (unsigned i = 0; i < count; ++i)
		{
			if (labels[i]->isA(CC_TYPES::LABEL_2D)) //Warning: cc2DViewportLabel is also a kind of 'CC_TYPES::LABEL_2D'!
			{
				cc2DLabel* label = static_cast<cc2DLabel*>(labels[i]);
				if (label->isVisible() && label->size() == 1)
				{
					pickedPoints.push_back(label);
				}
			}
		}
	}

	return static_cast<unsigned>(pickedPoints.size());
}

void ccLabelPathDlg::linkWithEntity(ccHObject* entity)
{
	m_associatedEntity = entity;
	m_lastPreviousID = 0;
	m_lastPathID = 0;

	if (m_associatedEntity)
	{
		//find default container
		m_orderedLabelsContainer = getLabelPathGroup(m_associatedEntity);

		std::vector<cc2DLabel*> previousPickedPoints;
		unsigned count = getPickedPoints(previousPickedPoints);
		//find highest unique ID among the VISIBLE labels
		for (unsigned i = 0; i < count; ++i)
		{
			m_lastPreviousID = std::max(m_lastPreviousID, previousPickedPoints[i]->getUniqueID());
			m_lastPathID = std::max(m_lastPathID, previousPickedPoints[i]->getLabelInfo().pathId.toUInt());
		}
	}

	ccShiftedObject* shifted = ccHObjectCaster::ToShifted(entity);
	showGlobalCoordsCheckBox->setEnabled(shifted ? shifted->isShifted() : false);
	updateList();
}

ccHObject* ccLabelPathDlg::getLabelPathGroup(ccHObject* entity)
{
	ccHObject::Container groups;
	entity->filterChildren(groups, true, CC_TYPES::HIERARCHY_OBJECT);

	for (ccHObject::Container::const_iterator it = groups.begin(); it != groups.end(); ++it)
	{
		if ((*it)->getName() == s_pickedPointContainerName)
		{
			return *it;
		}
	}
	return nullptr;
}

void ccLabelPathDlg::cancelAndExit()
{
	ccDBRoot* dbRoot = MainWindow::TheInstance()->db();
	if (!dbRoot)
	{
		assert(false);
		return;
	}

	if (m_orderedLabelsContainer)
	{
		//Restore previous state
		if (!m_toBeAdded.empty())
		{
			dbRoot->removeElements(m_toBeAdded);
		}

		for (auto & object : m_toBeDeleted)
		{
			object->prepareDisplayForRefresh();
			object->setEnabled(true);
		}

		if (m_orderedLabelsContainer->getChildrenNumber() == 0)
		{
			dbRoot->removeElement(m_orderedLabelsContainer);
			//m_associatedEntity->removeChild(m_orderedLabelsContainer);
			m_orderedLabelsContainer = nullptr;
		}
	}

	m_toBeDeleted.resize(0);
	m_toBeAdded.resize(0);
	m_associatedEntity = nullptr;
	m_orderedLabelsContainer = nullptr;

	MainWindow::RefreshAllGLWindow();

	updateList();

	stop(false);
}


void ccLabelPathDlg::applyAndExit()
{
	if (m_associatedEntity && !m_toBeDeleted.empty())
	{
		//apply modifications
		MainWindow::TheInstance()->db()->removeElements(m_toBeDeleted); //no need to redraw as they should already be invisible
		m_associatedEntity = nullptr;
	}

	m_toBeDeleted.resize(0);
	m_toBeAdded.resize(0);
	m_orderedLabelsContainer = nullptr;

	updateList();

	stop(true);
}

void ccLabelPathDlg::removeLastEntry()
{
	if (!m_associatedEntity)
		return;

	//get all labels
	std::vector<cc2DLabel*> labels;
	unsigned count = getPickedPoints(labels);
	if (count == 0)
		return;

	ccHObject* lastVisibleLabel = labels.back();
	if (lastVisibleLabel->getUniqueID() <= m_lastPreviousID)
	{
		//old label: hide it and add it to the 'to be deleted' list (will be restored if process is cancelled)
		lastVisibleLabel->setEnabled(false);
		m_toBeDeleted.push_back(lastVisibleLabel);
	}
	else
	{
		if (!m_toBeAdded.empty())
		{
			assert(m_toBeAdded.back() == lastVisibleLabel);
			m_toBeAdded.pop_back();
		}

		if (m_orderedLabelsContainer)
		{
			if (lastVisibleLabel->getParent())
			{
				lastVisibleLabel->getParent()->removeDependencyWith(lastVisibleLabel);
				lastVisibleLabel->removeDependencyWith(lastVisibleLabel->getParent());
			}
			//m_orderedLabelsContainer->removeChild(lastVisibleLabel);
			MainWindow::TheInstance()->db()->removeElement(lastVisibleLabel);		
		}
		else
			m_associatedEntity->detachChild(lastVisibleLabel);
		m_lastPathID--;
	}

	updateList();

	if (m_associatedWin)
		m_associatedWin->redraw();
}


void ccLabelPathDlg::markerSizeChanged(int size)
{
	if (size < 1 || !m_associatedWin)
		return;

	//display parameters
	ccGui::ParamStruct guiParams = m_associatedWin->getDisplayParameters();

	if (guiParams.labelMarkerSize != static_cast<unsigned>(size))
	{
		guiParams.labelMarkerSize = static_cast<unsigned>(size);
		m_associatedWin->setDisplayParameters(guiParams,m_associatedWin->hasOverriddenDisplayParameters());
		m_associatedWin->redraw();
	}
}



void ccLabelPathDlg::updateList()
{
	//get all labels
	std::vector<cc2DLabel*> labels;
	const int count = static_cast<int>( getPickedPoints(labels) );

	const int oldRowCount = tableWidget->rowCount();

	revertToolButton->setEnabled(count);
	validToolButton->setEnabled(count);
	countLineEdit->setText(QString::number(count));
	tableWidget->setRowCount(count);

	if ( count == 0 )
	{
		return;
	}

	// If we have any new rows, create QTableWidgetItems for them
	if ( (count - oldRowCount) > 0 )
	{
		for ( int i = oldRowCount; i < count; ++i )
		{
			tableWidget->setVerticalHeaderItem( i, new QTableWidgetItem );

			for ( int j = 0; j < 4; ++j )
			{
				tableWidget->setItem( i, j, new QTableWidgetItem );
			}
		}
	}
	const int precision = m_associatedWin ? static_cast<int>(m_associatedWin->getDisplayParameters().displayedNumPrecision) : 6;

	const bool showAbsolute = showGlobalCoordsCheckBox->isEnabled() && showGlobalCoordsCheckBox->isChecked();

	for ( int i = 0; i < count; ++i )
	{
		cc2DLabel* label = labels[static_cast<unsigned int>( i )];

		const cc2DLabel::PickedPoint& PP = label->getPickedPoint(0);
		CCVector3 P = PP.getPointPosition();
		CCVector3d Pd = (showAbsolute ? PP.cloudOrVertices()->toGlobal3d(P) : P);

		QString pathId = label->getLabelInfo().pathId;

		//update name as well
		if (	label->getUniqueID() > m_lastPreviousID
		    ||	label->getName().startsWith(s_defaultLabelBaseName) ) //DGM: we don't change the name of old labels that have a non-default name
		{
			label->setName(s_defaultLabelBaseName + pathId);
		}

		//point absolute index (in cloud)
		tableWidget->item( i, 0 )->setText( pathId );

		for ( int j = 0; j < 3; ++j )
		{
			tableWidget->item( i, j + 1 )->setText( QStringLiteral( "%1" ).arg( Pd.u[j], 0, 'f', precision ) );
		}
	}

	tableWidget->scrollToBottom();
}

void ccLabelPathDlg::processPickedPoint(const PickedItem& picked)
{
	if (!picked.entity || picked.entity != m_associatedEntity || !MainWindow::TheInstance())
		return;

	cc2DLabel* newLabel = new cc2DLabel();
	if (picked.entity->isKindOf(CC_TYPES::POINT_CLOUD))
	{
		newLabel->addPickedPoint(static_cast<ccGenericPointCloud*>(picked.entity), picked.itemIndex, picked.entityCenter);
	}
	else if (picked.entity->isKindOf(CC_TYPES::MESH))
	{
		newLabel->addPickedPoint(static_cast<ccGenericMesh*>(picked.entity), picked.itemIndex, CCVector2d(picked.uvw.x, picked.uvw.y), picked.entityCenter);
	}
	else
	{
		delete newLabel;
		assert(false);
		return;
	}
	newLabel->setVisible(true);
	newLabel->setDisplayedIn2D(false);
	newLabel->displayPointLegend(true);
	newLabel->setCollapsed(true);
	newLabel->setLabelInfo(LabelPathInfo{QString::number(++m_lastPathID)});
	ccGenericGLDisplay* display = m_associatedEntity->getDisplay();
	if (display)
	{
		newLabel->setDisplay(display);
		QSize size = display->getScreenSize();
		newLabel->setPosition(	static_cast<float>(picked.clickPoint.x() + 20) / size.width(),
		                        static_cast<float>(picked.clickPoint.y() + 20) / size.height() );
	}

	//add default container if necessary
	if (!m_orderedLabelsContainer)
	{
		m_orderedLabelsContainer = new ccHObject(s_pickedPointContainerName);
		m_associatedEntity->addChild(m_orderedLabelsContainer);
		m_orderedLabelsContainer->setDisplay(display);
		MainWindow::TheInstance()->addToDB(m_orderedLabelsContainer, false, true, false, false);
	}
	assert(m_orderedLabelsContainer);
	m_orderedLabelsContainer->addChild(newLabel);
	MainWindow::TheInstance()->addToDB(newLabel, false, true, false, false);
	m_toBeAdded.push_back(newLabel);

	//automatically send the new point coordinates to the clipboard
	QClipboard* clipboard = QApplication::clipboard();
	if (clipboard)
	{
		CCVector3 P = newLabel->getPickedPoint(0).getPointPosition();
		int precision = m_associatedWin ? m_associatedWin->getDisplayParameters().displayedNumPrecision : 6;
		int indexInList = 1 + static_cast<int>(m_orderedLabelsContainer->getChildrenNumber()) - 1;
		clipboard->setText(QString("CC_POINT_#%0(%1;%2;%3)").arg(indexInList).arg(P.x, 0, 'f', precision).arg(P.y, 0, 'f', precision).arg(P.z, 0, 'f', precision));
	}

	updateList();

	if (m_associatedWin)
		m_associatedWin->redraw();
}
