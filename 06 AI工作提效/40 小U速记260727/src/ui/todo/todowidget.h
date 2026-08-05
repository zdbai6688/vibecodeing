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

signals:
    void todoSelected(int todoId);
    void todoStatusChanged();

private slots:

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

    QLineEdit *m_newTodoInput;
    QListWidget *m_pendingList;
    QListWidget *m_completedList;
    QStringList m_filterTags;

    // 批量操作控件
    bool m_multiSelectMode = false;
    QWidget *m_batchToolbar;
    QPushButton *m_selectModeBtn;
    QPushButton *m_selectAllBtn;
    QPushButton *m_batchDeleteBtn;
    DLabel *m_selectionCountLabel;
};

#endif // TODOWIDGET_H
