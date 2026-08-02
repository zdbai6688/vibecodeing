// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TODOWIDGET_H
#define TODOWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDateEdit>
#include <QDate>
#include <DToolButton>
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
    void focusNewTodoInput();

signals:
    void todoSelected(int todoId);
    void todoStatusChanged();

private slots:
    void onContextMenu(const QPoint &pos);

private:
    void initUI();
    void populateSection(QListWidget *list, const QList<TodoData> &todos,
                         const QString &emptyHint, int &outCount,
                         bool isPreset = false);
    void updateOverallEmptyState();
    void setupListContextMenu(QListWidget *list);
    void loadSortPreference();
    void saveSortPreference();

    QMenu *createTodoContextMenu(int todoId);
    void buildTagSubMenu(QMenu *parentMenu, int todoId);
    void buildPrioritySubMenu(QMenu *parentMenu, int todoId);

    // 批量操作
    void enterMultiSelectMode();
    void exitMultiSelectMode();
    void updateSelectionState();
    void onBatchDelete();
    QList<int> getSelectedTodoIds() const;

    /// 返回预置示例待办列表（负 ID 标记，仅用于空状态引导）
    QList<TodoData> presetExamples() const;
    /// 日期选择器
    void showTodoEditDialog(int todoId);
    void updateDatePickerVisibility();
    void onCreateTodo();

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
    DToolButton *m_dateToggleBtn;
    QWidget *m_datePickerContainer;
    QDateEdit *m_dateEdit;
    QPushButton *m_todayBtn;
    QPushButton *m_tomorrowBtn;
    QPushButton *m_nextWeekBtn;
    QPushButton *m_clearDateBtn;
    qint64 m_pendingDueDate = 0;

    // 排序控件
    QComboBox *m_sortFieldCombo;
    DToolButton *m_sortOrderBtn;
    TodoSortParam m_sortParam;

    // 批量操作控件
    bool m_multiSelectMode = false;
    QWidget *m_batchToolbar;
    QPushButton *m_selectModeBtn;
    QPushButton *m_selectAllBtn;
    QPushButton *m_batchDeleteBtn;
    DLabel *m_selectionCountLabel;
};

#endif // TODOWIDGET_H
