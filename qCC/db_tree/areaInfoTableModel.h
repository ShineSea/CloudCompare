#pragma once
#include <QAbstractTableModel>
#include <QList>
#include "ccHObject.h" // AreaInfo定义在这里

class AreaInfoTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit AreaInfoTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    void addAreaInfo(const AreaLabelInfo& info);
    void removeAreaInfo(int row);
    AreaLabelInfo getAreaInfo(int row) const;
    void setAreaInfo(int row, const AreaLabelInfo& info);

    QList<AreaLabelInfo> getAll() const;

private:
    QList<AreaLabelInfo> m_data;
};
