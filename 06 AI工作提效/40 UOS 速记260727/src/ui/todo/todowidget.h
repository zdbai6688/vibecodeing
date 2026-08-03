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
#include <DLabel>
#include <DDialog>
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
    QStringList filterTags() const { return m_filterTags; }
    void focusNewTodoInput();

signals:
    void todoSelected(int todoId);
    void todoStatusChanged();

private slots:
    void onContextMenu(const QPoint &pos);

private:
    void initUI();
    void populateList(const QList<TodoData> &todos);
    QWidget *createTodoRow(const TodoData &todo);
    QWidget *createSectionHeader(const QString &title, const QString &color);
    void showTodoContextMenu(QListWidget *list, const QPoint &pos);
    void setTodoDueDate(int todoId);

    QLineEdit *m_newTodoInput;
    QListWidget *m_pendingList;
    QListWidget *m_completedList;
    QStringList m_filterTags;
};

#endif // TODOWIDGET_H