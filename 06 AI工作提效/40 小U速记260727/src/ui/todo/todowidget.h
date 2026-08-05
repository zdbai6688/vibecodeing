// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TODOWIDGET_H
#define TODOWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QMenu>
#include <QAction>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <DLabel>
#include <DDialog>
#include <DSpinner>
#include <QInputDialog>
#include "storage/todostorage.h"

DWIDGET_USE_NAMESPACE

// 极简待办列表：和笔记列表一致，只是每条前面有完成复选框
class TodoWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(TodoWidget)

public:
    explicit TodoWidget(QWidget *parent = nullptr);

    void refresh();
    void selectTodo(int todoId);
    void setFilterTags(const QStringList &tags);
    void clearFilterTags();
    QStringList filterTags() const { return m_filterTags; }
    void focusNewTodoInput();
    void setCalendarMode(bool on);
    bool calendarMode() const { return m_calendarMode; }

signals:
    void todoSelected(int todoId);
    void todoStatusChanged();

private slots:
    void onTodoDropped(int todoId, int dayIndex);

private:
    void initUI();
    void populateList(const QList<TodoData> &todos);
    QWidget *createTodoRow(const TodoData &todo);
    QWidget *createSectionHeader(const QString &title, const QString &color);
    void showTodoContextMenu(QListWidget *list, const QPoint &pos);
    void setTodoDueDate(int todoId);

    // 批量操作
    void enterMultiSelectMode();
    void exitMultiSelectMode();
    void updateSelectionState();
    void onBatchDelete();
    QList<int> getSelectedTodoIds() const;

    // 日程网格视图（Phase D）
    void initCalendarView();
    void populateCalendarView(const QList<TodoData> &todos);
    void updateCalendarWeekLabel();
    void onCalendarPrevWeek();
    void onCalendarNextWeek();
    QWidget *createTodoCard(const TodoData &todo);

    QLineEdit *m_newTodoInput;
    QListWidget *m_pendingList;
    QListWidget *m_completedList;
    QWidget *m_pendingHeader;
    QWidget *m_completedHeader;
    QStringList m_filterTags;

    // 日程网格视图控件
    QPushButton *m_calendarToggleBtn;
    QWidget *m_calendarView;
    QLabel *m_calendarWeekLabel;
    QPushButton *m_calendarPrevBtn;
    QPushButton *m_calendarNextBtn;
    QDate m_calendarMonday;
    QList<QListWidget *> m_dayLists;   // 周一~周日 7 列
    QList<QLabel *> m_calendarDateLabels; // 各列日期数字（今日高亮）
    QListWidget *m_unscheduledList;    // 未安排（无截止日期 + 其他周）
    bool m_calendarMode = false;

    // 批量操作控件
    bool m_multiSelectMode = false;
    QWidget *m_batchToolbar;
    QPushButton *m_selectModeBtn;
    QPushButton *m_selectAllBtn;
    QPushButton *m_batchDeleteBtn;
    DLabel *m_selectionCountLabel;
};

#endif // TODOWIDGET_H
