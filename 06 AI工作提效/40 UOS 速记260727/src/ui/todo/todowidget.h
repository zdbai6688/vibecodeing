#ifndef TODOWIDGET_H
#define TODOWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <DLabel>
#include "storage/todostorage.h"

DWIDGET_USE_NAMESPACE

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
    void populateSection(QListWidget *list, const QList<TodoData> &todos, const QString &emptyHint, int &outCount);
    void updateOverallEmptyState();

    QListWidget *m_todayList;
    QListWidget *m_overdueList;
    QListWidget *m_weekList;
    QListWidget *m_completedList;
    QVBoxLayout *m_mainLayout;
    QStackedWidget *m_stack;
    QWidget *m_contentWidget;
    QWidget *m_emptyWidget;
    int m_todayCount = 0;
    int m_overdueCount = 0;
    int m_weekCount = 0;
    int m_completedCount = 0;
};

#endif // TODOWIDGET_H