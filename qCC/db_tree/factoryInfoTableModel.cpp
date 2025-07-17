#include "factoryInfoTableModel.h"

FactoryInfoTableModel::FactoryInfoTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int FactoryInfoTableModel::rowCount(const QModelIndex&) const
{
    return m_data.size();
}

int FactoryInfoTableModel::columnCount(const QModelIndex&) const
{
    return 3; // factoryId, factoryName, voltageLevel
}

QVariant FactoryInfoTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_data.size())
        return QVariant();

    const FactoryLabelInfo& info = m_data.at(index.row());
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch (index.column())
        {
        case 0: return info.factoryId;
        case 1: return info.factoryName;
        case 2: return info.voltageLevel;
        }
    }
    return QVariant();
}

QVariant FactoryInfoTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0: return QStringLiteral("厂站ID");
        case 1: return QStringLiteral("厂站名称");
        case 2: return QStringLiteral("电压等级");
        }
    }
    return QVariant();
}

Qt::ItemFlags FactoryInfoTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;
}

bool FactoryInfoTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() >= m_data.size() || role != Qt::EditRole)
        return false;

    FactoryLabelInfo& info = m_data[index.row()];
    switch (index.column())
    {
    case 0: info.factoryId = value.toString(); break;
    case 1: info.factoryName = value.toString(); break;
    case 2: info.voltageLevel = value.toString(); break;
    default: return false;
    }
    emit dataChanged(index, index);
    return true;
}

void FactoryInfoTableModel::addFactoryInfo(const FactoryLabelInfo& info)
{
    beginInsertRows(QModelIndex(), m_data.size(), m_data.size());
    m_data.append(info);
    endInsertRows();
}

void FactoryInfoTableModel::removeFactoryInfo(int row)
{
    if (row < 0 || row >= m_data.size()) return;
    beginRemoveRows(QModelIndex(), row, row);
    m_data.removeAt(row);
    endRemoveRows();
}

FactoryLabelInfo FactoryInfoTableModel::getFactoryInfo(int row) const
{
    if (row < 0 || row >= m_data.size()) return FactoryLabelInfo();
    return m_data.at(row);
}

void FactoryInfoTableModel::setFactoryInfo(int row, const FactoryLabelInfo& info)
{
    if (row < 0 || row >= m_data.size()) return;
    m_data[row] = info;
    emit dataChanged(index(row, 0), index(row, columnCount() - 1));
}

QList<FactoryLabelInfo> FactoryInfoTableModel::getAll() const
{
    return m_data;
}
