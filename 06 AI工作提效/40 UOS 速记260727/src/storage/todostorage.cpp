// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "todostorage.h"
#include "storage/database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include <QDate>
#include <QSqlDatabase>

// Helper: QSqlQuery → QVariantMap
static QVariantMap queryToMap(const QSqlQuery &query)
{
    QVariantMap map;
    for (int i = 0; i < query.record().count(); ++i) {
        map[query.record().fieldName(i)] = query.value(i);
    }
    return map;
}

bool TodoData::isOverdue() const
{
    if (isCompleted || dueDatetime == 0) return false;
    return QDateTime::currentSecsSinceEpoch() > dueDatetime;
}

bool TodoData::isDueToday() const
{
    if (dueDatetime == 0) return false;
    QDate today = QDate::currentDate();
    QDate due = QDateTime::fromSecsSinceEpoch(dueDatetime).date();
    return due == today;
}

bool TodoData::isDueThisWeek() const
{
    if (dueDatetime == 0) return false;
    QDate today = QDate::currentDate();
    QDate due = QDateTime::fromSecsSinceEpoch(dueDatetime).date();
    return due >= today.addDays(-(int)today.dayOfWeek() + 1)
        && due <= today.addDays(7 - (int)today.dayOfWeek());
}

TodoStorage::TodoStorage(Database *db, QObject *parent)
    : QObject(parent), m_db(db)
{
}

int TodoStorage::createTodo(const TodoData &todo)
{
    QSqlQuery query(m_db->connection());
    query.prepare(R"(
        INSERT INTO notes_todos (title, content, is_todo, priority, is_completed, tag,
                                 due_datetime, completed_datetime, creation_datetime, modification_datetime)
        VALUES (:title, :content, 1, :priority, :completed, :tag,
                :due, :completed_at, :created, :modified)
    )");
    query.bindValue(":title", todo.title);
    query.bindValue(":content", todo.content);
    query.bindValue(":priority", todo.priority);
    query.bindValue(":completed", todo.isCompleted ? 1 : 0);
    query.bindValue(":tag", todo.tag);
    query.bindValue(":due", todo.dueDatetime);
    query.bindValue(":completed_at", todo.completedDatetime);
    query.bindValue(":created", todo.creationDatetime > 0 ? todo.creationDatetime : QDateTime::currentSecsSinceEpoch());
    query.bindValue(":modified", todo.modificationDatetime > 0 ? todo.modificationDatetime : QDateTime::currentSecsSinceEpoch());

    if (!query.exec()) {
        qWarning() << "创建待办失败:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

bool TodoStorage::updateTodo(const TodoData &todo)
{
    QSqlQuery query(m_db->connection());
    query.prepare(R"(
        UPDATE notes_todos SET title=:title, content=:content, priority=:priority,
               is_completed=:completed, tag=:tag, due_datetime=:due,
               completed_datetime=:completed_at, modification_datetime=:modified
        WHERE id=:id AND is_todo=1
    )");
    query.bindValue(":title", todo.title);
    query.bindValue(":content", todo.content);
    query.bindValue(":priority", todo.priority);
    query.bindValue(":completed", todo.isCompleted ? 1 : 0);
    query.bindValue(":tag", todo.tag);
    query.bindValue(":due", todo.dueDatetime);
    query.bindValue(":completed_at", todo.completedDatetime);
    query.bindValue(":modified", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", todo.id);

    if (!query.exec()) {
        qWarning() << "更新待办失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool TodoStorage::toggleComplete(int id, bool completed)
{
    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE notes_todos SET is_completed=:c, completed_datetime=:t, modification_datetime=:m WHERE id=:id AND is_todo=1");
    query.bindValue(":c", completed ? 1 : 0);
    query.bindValue(":t", completed ? QDateTime::currentSecsSinceEpoch() : 0);
    query.bindValue(":m", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", id);
    return query.exec();
}

bool TodoStorage::setPriority(int id, int priority)
{
    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE notes_todos SET priority=:p, modification_datetime=:m WHERE id=:id AND is_todo=1");
    query.bindValue(":p", priority);
    query.bindValue(":m", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", id);
    return query.exec();
}
bool TodoStorage::setTag(int id, const QString &tag)
{
    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE notes_todos SET tag=:t, modification_datetime=:m WHERE id=:id AND is_todo=1");
    query.bindValue(":t", tag);
    query.bindValue(":m", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", id);
    if (!query.exec()) {
        qWarning() << "设置待办标签失败:" << query.lastError().text();
        return false;
    }
    return true;
}


bool TodoStorage::deleteTodo(int id)
{
    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE notes_todos SET is_deleted=1, deletion_datetime=:d, modification_datetime=:m WHERE id=:id AND is_todo=1");
    query.bindValue(":d", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":m", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", id);
    if (!query.exec()) {
        qWarning() << "删除待办失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool TodoStorage::restoreTodo(int id)
{
    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE notes_todos SET is_deleted=0, deletion_datetime=0, modification_datetime=:m WHERE id=:id AND is_todo=1");
    query.bindValue(":m", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", id);
    return query.exec();
}

bool TodoStorage::permanentDelete(int id)
{
    QSqlQuery query(m_db->connection());
    query.prepare("DELETE FROM notes_todos WHERE id=:id AND is_deleted=1 AND is_todo=1");
    query.bindValue(":id", id);
    return query.exec();
}

// ─── 批量操作 ─────────────────────────────────────────────────────

bool TodoStorage::batchDeleteTodos(const QList<int> &ids)
{
    if (ids.isEmpty()) return true;

    QSqlDatabase db = QSqlDatabase::database(m_db->connection().connectionName());
    if (!db.transaction()) {
        qWarning() << "batchDeleteTodos: 无法开始事务";
        return false;
    }

    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE notes_todos SET is_deleted=1, deletion_datetime=:d, modification_datetime=:m WHERE id=:id AND is_todo=1");

    qint64 now = QDateTime::currentSecsSinceEpoch();
    for (int id : ids) {
        query.bindValue(":d", now);
        query.bindValue(":m", now);
        query.bindValue(":id", id);
        if (!query.exec()) {
            qWarning() << "batchDeleteTodos: 删除待办" << id << "失败:" << query.lastError().text();
            db.rollback();
            return false;
        }
    }

    return db.commit();
}

bool TodoStorage::batchRestoreTodos(const QList<int> &ids)
{
    if (ids.isEmpty()) return true;

    QSqlDatabase db = QSqlDatabase::database(m_db->connection().connectionName());
    if (!db.transaction()) {
        qWarning() << "batchRestoreTodos: 无法开始事务";
        return false;
    }

    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE notes_todos SET is_deleted=0, deletion_datetime=0, modification_datetime=:m WHERE id=:id AND is_todo=1");

    qint64 now = QDateTime::currentSecsSinceEpoch();
    for (int id : ids) {
        query.bindValue(":m", now);
        query.bindValue(":id", id);
        if (!query.exec()) {
            qWarning() << "batchRestoreTodos: 恢复待办" << id << "失败:" << query.lastError().text();
            db.rollback();
            return false;
        }
    }

    return db.commit();
}

// ─── 查询 ─────────────────────────────────────────────────────────

TodoData TodoStorage::getTodo(int id) const
{
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM notes_todos WHERE id=:id AND is_todo=1");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        return rowToTodo(queryToMap(query));
    }
    return TodoData();
}


QString TodoStorage::buildTodoOrderClause(const TodoSortParam &sort) const
{
    QString fieldName = (sort.field == TodoSortParam::CreatedAt)
        ? "creation_datetime" : "due_datetime";
    QString order = sort.ascending ? "ASC" : "DESC";
    return QString(" ORDER BY %1 %2").arg(fieldName, order);
}


QList<TodoData> TodoStorage::getAllTodos(bool includeCompleted, bool includeDeleted) const
{
    QList<TodoData> list;
    QSqlQuery query(m_db->connection());
    QString sql = "SELECT * FROM notes_todos WHERE is_todo=1";
    if (!includeCompleted) sql += " AND is_completed=0";
    if (!includeDeleted) sql += " AND is_deleted=0";
    sql += " ORDER BY modification_datetime DESC";
    if (query.exec(sql)) {
        while (query.next()) list.append(rowToTodo(queryToMap(query)));
    }
    return list;
}


QList<TodoData> TodoStorage::getPendingTodos(const TodoSortParam &sort) const
{
    QList<TodoData> list;
    QSqlQuery query(m_db->connection());
    QString sql = "SELECT * FROM notes_todos WHERE is_todo=1 AND is_completed=0 AND is_deleted=0"
                  + buildTodoOrderClause(sort);
    if (query.exec(sql)) {
        while (query.next()) list.append(rowToTodo(queryToMap(query)));
    }
    return list;
}

QList<TodoData> TodoStorage::getCompletedTodos() const
{
    QList<TodoData> list;
    QSqlQuery query(m_db->connection());
    if (query.exec("SELECT * FROM notes_todos WHERE is_todo=1 AND is_completed=1 AND is_deleted=0 ORDER BY completed_datetime DESC")) {
        while (query.next()) list.append(rowToTodo(queryToMap(query)));
    }
    return list;
}

QList<TodoData> TodoStorage::getDeletedTodos() const
{
    QList<TodoData> list;
    QSqlQuery query(m_db->connection());
    if (query.exec("SELECT * FROM notes_todos WHERE is_todo=1 AND is_deleted=1 ORDER BY deletion_datetime DESC")) {
        while (query.next()) list.append(rowToTodo(queryToMap(query)));
    }
    return list;
}

QList<TodoData> TodoStorage::getTodayTodos(const TodoSortParam &sort) const
{
    QList<TodoData> list;
    qint64 todayStart = QDateTime(QDate::currentDate(), QTime(0,0)).toSecsSinceEpoch();
    qint64 todayEnd = QDateTime(QDate::currentDate(), QTime(23,59,59)).toSecsSinceEpoch();
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM notes_todos WHERE is_todo=1 AND is_deleted=0 AND is_completed=0 AND due_datetime>=:start AND due_datetime<=:end"
                  + buildTodoOrderClause(sort));
    query.bindValue(":start", todayStart);
    query.bindValue(":end", todayEnd);
    if (query.exec()) {
        while (query.next()) list.append(rowToTodo(queryToMap(query)));
    }
    return list;
}

QList<TodoData> TodoStorage::getOverdueTodos(const TodoSortParam &sort) const
{
    QList<TodoData> list;
    qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM notes_todos WHERE is_todo=1 AND is_deleted=0 AND is_completed=0 AND due_datetime>0 AND due_datetime<:now"
                  + buildTodoOrderClause(sort));
    query.bindValue(":now", now);
    if (query.exec()) {
        while (query.next()) list.append(rowToTodo(queryToMap(query)));
    }
    return list;
}

QList<TodoData> TodoStorage::getWeekTodos(const TodoSortParam &sort) const
{
    QList<TodoData> list;
    QDate today = QDate::currentDate();
    QDate weekStart = today.addDays(-(int)today.dayOfWeek() + 1); // 周一
    QDate weekEnd = today.addDays(7 - (int)today.dayOfWeek());     // 周日
    qint64 startSecs = QDateTime(weekStart, QTime(0, 0, 0)).toSecsSinceEpoch();
    qint64 endSecs = QDateTime(weekEnd, QTime(23, 59, 59)).toSecsSinceEpoch();
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM notes_todos WHERE is_todo=1 AND is_deleted=0 AND is_completed=0 AND due_datetime>0 AND due_datetime>=:start AND due_datetime<=:end"
                  + buildTodoOrderClause(sort));
    query.bindValue(":start", startSecs);
    query.bindValue(":end", endSecs);
    if (query.exec()) {
        while (query.next()) list.append(rowToTodo(queryToMap(query)));
    }
    return list;
}

QList<TodoData> TodoStorage::getTodosByTag(const QString &tag) const
{
    QList<TodoData> list;
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM notes_todos WHERE is_todo=1 AND is_deleted=0 AND tag LIKE :tag ORDER BY modification_datetime DESC");
    query.bindValue(":tag", "%" + tag + "%");
    if (query.exec()) {
        while (query.next()) list.append(rowToTodo(queryToMap(query)));
    }
    return list;
}

QList<TodoData> TodoStorage::searchTodos(const QString &keyword) const
{
    QList<TodoData> list;
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM notes_todos WHERE is_todo=1 AND is_deleted=0 AND (title LIKE :kw OR content LIKE :kw2) ORDER BY modification_datetime DESC");
    query.bindValue(":kw", "%" + keyword + "%");
    query.bindValue(":kw2", "%" + keyword + "%");
    if (query.exec()) {
        while (query.next()) list.append(rowToTodo(queryToMap(query)));
    }
    return list;
}

int TodoStorage::pendingCount() const
{
    QSqlQuery query(m_db->connection());
    if (query.exec("SELECT COUNT(*) FROM notes_todos WHERE is_todo=1 AND is_completed=0 AND is_deleted=0")) {
        if (query.next()) return query.value(0).toInt();
    }
    return 0;
}

int TodoStorage::completedCount() const
{
    QSqlQuery query(m_db->connection());
    if (query.exec("SELECT COUNT(*) FROM notes_todos WHERE is_todo=1 AND is_completed=1 AND is_deleted=0")) {
        if (query.next()) return query.value(0).toInt();
    }
    return 0;
}

TodoData TodoStorage::rowToTodo(const QVariantMap &row) const
{
    TodoData todo;
    todo.id = row.value("id").toInt();
    todo.noteId = row.value("id").toInt();
    todo.title = row.value("title").toString();
    todo.content = row.value("content").toString();
    todo.priority = row.value("priority").toInt();
    todo.isCompleted = row.value("is_completed").toInt() == 1;
    todo.tag = row.value("tag").toString();
    todo.dueDatetime = row.value("due_datetime").toLongLong();
    todo.completedDatetime = row.value("completed_datetime").toLongLong();
    todo.creationDatetime = row.value("creation_datetime").toLongLong();
    todo.modificationDatetime = row.value("modification_datetime").toLongLong();
    todo.isDeleted = row.value("is_deleted").toInt() == 1;
    return todo;
}
