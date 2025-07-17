#pragma once
#include <QAbstractTableModel>
#include <QList>
#include "ccHObject.h"

class FactoryInfoTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit FactoryInfoTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    // 增加、删除
    void addFactoryInfo(const FactoryLabelInfo& info);
    void removeFactoryInfo(int row);
    FactoryLabelInfo getFactoryInfo(int row) const;
    void setFactoryInfo(int row, const FactoryLabelInfo& info);

    QList<FactoryLabelInfo> getAll() const;

private:
    QList<FactoryLabelInfo> m_data;
};
