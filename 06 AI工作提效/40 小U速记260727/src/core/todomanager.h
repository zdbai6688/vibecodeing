// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TODOMANAGER_H
#define TODOMANAGER_H

#include <QObject>
#include "storage/todostorage.h"

class Database;

class TodoManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(TodoManager)

public:
    explicit TodoManager(Database *db, QObject *parent = nullptr);

    int createTodo(const TodoData &todo);
    bool updateTodo(const TodoData &todo);
    bool toggleComplete(int id, bool completed);
    bool setPriority(int id, int priority);
    bool setTag(int id, const QString &tag);
    bool deleteTodo(int id);
    bool restoreTodo(int id);
    bool permanentDelete(int id);

    // 批量操作
    bool batchDeleteTodos(const QList<int> &ids);
    bool batchRestoreTodos(const QList<int> &ids);

    TodoData getTodo(int id) const;
    QList<TodoData> getAllTodos() const;
    QList<TodoData> getPendingTodos(const TodoSortParam &sort = TodoSortParam()) const;
    QList<TodoData> getCompletedTodos() const;
    QList<TodoData> getDeletedTodos() const;
    QList<TodoData> getTodayTodos(const TodoSortParam &sort = TodoSortParam()) const;
    QList<TodoData> getOverdueTodos(const TodoSortParam &sort = TodoSortParam()) const;
    QList<TodoData> getWeekTodos(const TodoSortParam &sort = TodoSortParam()) const;
    QList<TodoData> getTodosByTag(const QString &tag) const;
    QList<TodoData> getTodosByTags(const QStringList &tags) const;
    QList<TagStat> getTagStats(qint64 startSecs, qint64 endSecs) const; // 周报标签统计
    QList<TodoData> searchTodos(const QString &keyword) const;

    int pendingCount() const;
    int completedCount() const;

    // 多标签支持
    QStringList getTodoTags(int id) const;
    bool setTodoTags(int id, const QStringList &tags);
    bool addTodoTag(int id, int tagId);
    bool removeTodoTag(int id, int tagId);

signals:
    void todoCreated(int id);
    void todoUpdated(int id);
    void todoDeleted(int id);
    void todoRestored(int id);
    void todoToggled(int id, bool completed);
    void dataChanged();

private:
    TodoStorage *m_storage;
};

#endif // TODOMANAGER_H
