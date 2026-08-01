#ifndef TODOWIDGET_H
#define TODOWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QLabel>
#include "storage/todostorage.h"

class TodoWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(TodoWidget)

public:
    explicit TodoWidget(QWidget *parent = nullptr);

    void refresh();
    void selectTodo(int todoId);

signals:
    void todoSelected(int todoId);
    void todoStatusChanged();

private:
    void initUI();
    void populateSection(QListWidget *list, const QList<TodoData> &todos, const QString &emptyHint);

    QListWidget *m_todayList;
    QListWidget *m_overdueList;
    QListWidget *m_weekList;
    QListWidget *m_completedList;
    QVBoxLayout *m_mainLayout;
};

#endif // TODOWIDGET_H