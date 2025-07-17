#include "areaInfoTableModel.h"
#include "areaEditDlg.h"

AreaInfoTableModel::AreaInfoTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int AreaInfoTableModel::rowCount(const QModelIndex&) const
{
    return m_data.size();
}

int AreaInfoTableModel::columnCount(const QModelIndex&) const
{
    return 2; // areaId, areaName
}

QVariant AreaInfoTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_data.size())
        return QVariant();

    const AreaLabelInfo& info = m_data.at(index.row());
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch (index.column())
        {
        case 0: return info.areaId;
        case 1: return info.areaName;
        }
    }
    return QVariant();
}

QVariant AreaInfoTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0: return QStringLiteral("区域ID");
        case 1: return QStringLiteral("区域名称");
        }
    }
    return QVariant();
}

Qt::ItemFlags AreaInfoTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;
}

bool AreaInfoTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() >= m_data.size() || role != Qt::EditRole)
        return false;

    AreaLabelInfo& info = m_data[index.row()];
    switch (index.column())
    {
    case 0: info.areaId = value.toString(); break;
    case 1: info.areaName = value.toString(); break;
    default: return false;
    }
    emit dataChanged(index, index);
    return true;
}

void AreaInfoTableModel::addAreaInfo(const AreaLabelInfo& info)
{
    beginInsertRows(QModelIndex(), m_data.size(), m_data.size());
    m_data.append(info);
    endInsertRows();
}


void AreaInfoTableModel::removeAreaInfo(int row)
{
    if (row < 0 || row >= m_data.size()) return;
    beginRemoveRows(QModelIndex(), row, row);
    m_data.removeAt(row);
    endRemoveRows();
}

AreaLabelInfo AreaInfoTableModel::getAreaInfo(int row) const
{
    if (row < 0 || row >= m_data.size()) return AreaLabelInfo();
    return m_data.at(row);
}

void AreaInfoTableModel::setAreaInfo(int row, const AreaLabelInfo& info)
{
    if (row < 0 || row >= m_data.size()) return;
    m_data[row] = info;
    emit dataChanged(index(row, 0), index(row, columnCount() - 1));
}

QList<AreaLabelInfo> AreaInfoTableModel::getAll() const
{
    return m_data;
}
