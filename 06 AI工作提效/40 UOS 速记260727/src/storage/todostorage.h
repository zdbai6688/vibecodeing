// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TODOSTORAGE_H
#define TODOSTORAGE_H

#include <QObject>
#include <QList>
#include <QDateTime>

class Database;

struct TodoData {
    int id = 0;
    int noteId = 0;       // 关联笔记ID（0表示独立待办）
    QString title;
    QString content;
    int priority = 0;     // 0=无, 1=低, 2=中, 3=高
    bool isCompleted = false;
    QString tag;
    qint64 dueDatetime = 0;
    qint64 completedDatetime = 0;
    qint64 creationDatetime = 0;
    qint64 modificationDatetime = 0;
    bool isDeleted = false;

    QDateTime dueDate() const { return QDateTime::fromSecsSinceEpoch(dueDatetime); }
    QDateTime createdAt() const { return QDateTime::fromSecsSinceEpoch(creationDatetime); }
    bool isOverdue() const;
    bool isDueToday() const;
    bool isDueThisWeek() const;
};

class TodoStorage : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(TodoStorage)

public:
    explicit TodoStorage(Database *db, QObject *parent = nullptr);

    int createTodo(const TodoData &todo);
    bool updateTodo(const TodoData &todo);
    bool toggleComplete(int id, bool completed);
    bool setPriority(int id, int priority);
    bool deleteTodo(int id);
    bool restoreTodo(int id);
    bool permanentDelete(int id);

    TodoData getTodo(int id) const;
    QList<TodoData> getAllTodos(bool includeCompleted = false, bool includeDeleted = false) const;
    QList<TodoData> getPendingTodos() const;
    QList<TodoData> getCompletedTodos() const;
    QList<TodoData> getDeletedTodos() const;
    QList<TodoData> getTodayTodos() const;
    QList<TodoData> getOverdueTodos() const;
    QList<TodoData> getTodosByTag(const QString &tag) const;
    QList<TodoData> searchTodos(const QString &keyword) const;

    int pendingCount() const;
    int completedCount() const;

private:
    TodoData rowToTodo(const QVariantMap &row) const;
    Database *m_db;
};

#endif // TODOSTORAGE_H