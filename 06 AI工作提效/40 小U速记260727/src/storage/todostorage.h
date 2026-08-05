// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TODOSTORAGE_H
#define TODOSTORAGE_H

#include <QObject>
#include <QList>
#include <QStringList>
#include <QDateTime>

class Database;

struct TodoData {
    int id = 0;
    int noteId = 0;       // 关联笔记ID（0表示独立待办）
    QString title;
    QString content;
    int priority = 0;     // 0=无, 1=低, 2=中, 3=高
    bool isCompleted = false;
    QString tag;          // 单标签（兼容旧数据）
    QStringList tags;     // 多标签（v2+）
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

// 标签统计（周报标签维度）
struct TagStat {
    QString tag;       // 标签名（空串/未分类用 "未分类" 占位）
    int total = 0;     // 该标签在时间范围内的待办总数
    int completed = 0; // 已完成数
    double rate() const { return total > 0 ? completed * 100.0 / total : 0.0; }
};

// 待办排序参数
struct TodoSortParam {
    enum Field { DueDate = 0, CreatedAt };
    Field field = DueDate;
    bool ascending = true; // true = ASC（默认：截止日期升序）
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
    bool setTag(int id, const QString &tag);
    bool deleteTodo(int id);
    bool restoreTodo(int id);
    bool permanentDelete(int id);

    // 批量操作
    bool batchDeleteTodos(const QList<int> &ids);
    bool batchRestoreTodos(const QList<int> &ids);

    TodoData getTodo(int id) const;
    QList<TodoData> getAllTodos(bool includeCompleted = false, bool includeDeleted = false) const;
    QList<TodoData> getPendingTodos(const TodoSortParam &sort = TodoSortParam()) const;
    QList<TodoData> getCompletedTodos() const;
    QList<TodoData> getDeletedTodos() const;
    QList<TodoData> getTodayTodos(const TodoSortParam &sort = TodoSortParam()) const;
    QList<TodoData> getOverdueTodos(const TodoSortParam &sort = TodoSortParam()) const;
    QList<TodoData> getWeekTodos(const TodoSortParam &sort = TodoSortParam()) const;
    QList<TodoData> getTodosByTag(const QString &tag) const;
    QList<TodoData> getTodosByTags(const QStringList &tags) const; // 多标签筛选
    QList<TagStat> getTagStats(qint64 startSecs, qint64 endSecs) const; // 按生效日期范围统计各标签完成情况

    QList<TodoData> searchTodos(const QString &keyword) const;

    int pendingCount() const;
    int completedCount() const;

    // 多标签支持
    QStringList getTodoTags(int id) const;         // 获取待办的多标签
    bool setTodoTags(int id, const QStringList &tags); // 设置待办的多标签（全量替换）
    bool addTodoTag(int id, int tagId);             // 添加单标签
    bool removeTodoTag(int id, int tagId);           // 移除单标签
    QStringList getTagsStringList(int id) const;     // 获取标签名列表

private:
    TodoData rowToTodo(const QVariantMap &row) const;
    QString buildTodoOrderClause(const TodoSortParam &sort) const;
    Database *m_db;
};

#endif // TODOSTORAGE_H
