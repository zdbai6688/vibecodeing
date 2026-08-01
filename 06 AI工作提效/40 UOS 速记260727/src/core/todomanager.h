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
    bool deleteTodo(int id);
    bool restoreTodo(int id);
    bool permanentDelete(int id);

    TodoData getTodo(int id) const;
    QList<TodoData> getAllTodos() const;
    QList<TodoData> getPendingTodos() const;
    QList<TodoData> getCompletedTodos() const;
    QList<TodoData> getDeletedTodos() const;
    QList<TodoData> getTodayTodos() const;
    QList<TodoData> getOverdueTodos() const;
    QList<TodoData> getTodosByTag(const QString &tag) const;
    QList<TodoData> searchTodos(const QString &keyword) const;

    int pendingCount() const;
    int completedCount() const;

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