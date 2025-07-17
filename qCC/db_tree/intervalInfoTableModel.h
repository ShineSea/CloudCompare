#pragma once
#include <QAbstractTableModel>
#include <QList>
#include "ccHObject.h" // IntervalInfo定义在这里

class IntervalInfoTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit IntervalInfoTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    void addIntervalInfo(const IntervalLabelInfo& info);
    void removeIntervalInfo(int row);
    IntervalLabelInfo getIntervalInfo(int row) const;
    void setIntervalInfo(int row, const IntervalLabelInfo& info);

    QList<IntervalLabelInfo> getAll() const;

private:
    QList<IntervalLabelInfo> m_data;
};
