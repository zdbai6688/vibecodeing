#ifndef TODOWIDGET_H
#define TODOWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QLineEdit>
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
    void populateSection(QListWidget *list, const QList<TodoData> &todos,
                         const QString &emptyHint, int &outCount,
                         bool isPreset = false);
    void updateOverallEmptyState();

    /// 返回预置示例待办列表（负 ID 标记，仅用于空状态引导）
    QList<TodoData> presetExamples() const;
    /// 按 section 名称过滤预置示例
    QList<TodoData> presetExamplesForSection(const QString &section) const;

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
    QLineEdit *m_newTodoInput;
};

#endif // TODOWIDGET_H
