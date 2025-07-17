#include "intervalInfoTableModel.h"

IntervalInfoTableModel::IntervalInfoTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int IntervalInfoTableModel::rowCount(const QModelIndex&) const
{
    return m_data.size();
}

int IntervalInfoTableModel::columnCount(const QModelIndex&) const
{
    return 2; // intervalId, intervalName
}

QVariant IntervalInfoTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_data.size())
        return QVariant();

    const IntervalLabelInfo& info = m_data.at(index.row());
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch (index.column())
        {
        case 0: return info.intervalId;
        case 1: return info.intervalName;
        }
    }
    return QVariant();
}

QVariant IntervalInfoTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0: return QStringLiteral("间隔ID");
        case 1: return QStringLiteral("间隔名称");
        }
    }
    return QVariant();
}

Qt::ItemFlags IntervalInfoTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;
}

bool IntervalInfoTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() >= m_data.size() || role != Qt::EditRole)
        return false;

    IntervalLabelInfo& info = m_data[index.row()];
    switch (index.column())
    {
    case 0: info.intervalId = value.toString(); break;
    case 1: info.intervalName = value.toString(); break;
    default: return false;
    }
    emit dataChanged(index, index);
    return true;
}

void IntervalInfoTableModel::addIntervalInfo(const IntervalLabelInfo& info)
{
    beginInsertRows(QModelIndex(), m_data.size(), m_data.size());
    m_data.append(info);
    endInsertRows();
}

void IntervalInfoTableModel::removeIntervalInfo(int row)
{
    if (row < 0 || row >= m_data.size()) return;
    beginRemoveRows(QModelIndex(), row, row);
    m_data.removeAt(row);
    endRemoveRows();
}

IntervalLabelInfo IntervalInfoTableModel::getIntervalInfo(int row) const
{
    if (row < 0 || row >= m_data.size()) return IntervalLabelInfo();
    return m_data.at(row);
}

void IntervalInfoTableModel::setIntervalInfo(int row, const IntervalLabelInfo& info)
{
    if (row < 0 || row >= m_data.size()) return;
    m_data[row] = info;
    emit dataChanged(index(row, 0), index(row, columnCount() - 1));
}

QList<IntervalLabelInfo> IntervalInfoTableModel::getAll() const
{
    return m_data;
}
