#pragma once

#include <QTableView>
#include <QStandardItemModel>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>

class BaseEditTableView : public QTableView
{
    Q_OBJECT
public:
    explicit BaseEditTableView(QWidget* parent = nullptr);
protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
private slots:
    virtual void onDelete();
	virtual void onAdd();
protected:
    QMenu* m_menu;
    QAction* m_addAction;
    QAction* m_deleteAction;
};
