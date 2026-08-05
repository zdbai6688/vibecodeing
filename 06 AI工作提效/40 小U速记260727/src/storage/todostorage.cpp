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
#include <QSet>

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
    // 多标签：取第一个标签写入旧字段，或清空
    query.bindValue(":tag", todo.tags.isEmpty() ? todo.tag : todo.tags.first());
    query.bindValue(":due", todo.dueDatetime);
    query.bindValue(":completed_at", todo.completedDatetime);
    query.bindValue(":created", todo.creationDatetime > 0 ? todo.creationDatetime : QDateTime::currentSecsSinceEpoch());
    query.bindValue(":modified", todo.modificationDatetime > 0 ? todo.modificationDatetime : QDateTime::currentSecsSinceEpoch());

    if (!query.exec()) {
        qWarning() << "创建待办失败:" << query.lastError().text();
        return -1;
    }
    int id = query.lastInsertId().toInt();

    // 写入多标签关联
    if (id > 0 && !todo.tags.isEmpty()) {
        setTodoTags(id, todo.tags);
    }

    return id;
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
    query.bindValue(":tag", todo.tags.isEmpty() ? todo.tag : todo.tags.first());
    query.bindValue(":due", todo.dueDatetime);
    query.bindValue(":completed_at", todo.completedDatetime);
    query.bindValue(":modified", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", todo.id);

    if (!query.exec()) {
        qWarning() << "更新待办失败:" << query.lastError().text();
        return false;
    }

    // 同步多标签关联
    if (!todo.tags.isEmpty()) {
        setTodoTags(todo.id, todo.tags);
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
    // 同步多标签：如果 tag 非空，替换 todo_tags
    if (!tag.isEmpty()) {
        setTodoTags(id, {tag});
    }
    return true;
}


bool TodoStorage::deleteTodo(int id)
{
    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE notes_todos SET is_deleted=1, deletion_datetime=:dt, modification_datetime=:dt WHERE id=:id AND is_todo=1");
    query.bindValue(":dt", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", id);
    return query.exec();
}

bool TodoStorage::restoreTodo(int id)
{
    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE notes_todos SET is_deleted=0, modification_datetime=:dt WHERE id=:id AND is_todo=1");
    query.bindValue(":dt", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", id);
    return query.exec();
}

bool TodoStorage::permanentDelete(int id)
{
    // 同时删除关联标签
    QSqlQuery delTags(m_db->connection());
    delTags.prepare("DELETE FROM todo_tags WHERE todo_id=:id");
    delTags.bindValue(":id", id);
    delTags.exec();

    QSqlQuery query(m_db->connection());
    query.prepare("DELETE FROM notes_todos WHERE id=:id AND is_todo=1");
    query.bindValue(":id", id);
    return query.exec();
}

// ─── 批量操作 ──────────────────────────────────────────────────

bool TodoStorage::batchDeleteTodos(const QList<int> &ids)
{
    QSqlQuery query(m_db->connection());
    qint64 now = QDateTime::currentSecsSinceEpoch();
    for (int id : ids) {
        query.prepare("UPDATE notes_todos SET is_deleted=1, deletion_datetime=:dt, modification_datetime=:dt2 WHERE id=:id AND is_todo=1");
        query.bindValue(":dt", now);
        query.bindValue(":dt2", now);
        query.bindValue(":id", id);
        if (!query.exec()) {
            qWarning() << "批量删除待办失败 id=" << id;
            return false;
        }
    }
    return true;
}

bool TodoStorage::batchRestoreTodos(const QList<int> &ids)
{
    QSqlQuery query(m_db->connection());
    qint64 now = QDateTime::currentSecsSinceEpoch();
    for (int id : ids) {
        query.prepare("UPDATE notes_todos SET is_deleted=0, modification_datetime=:dt WHERE id=:id AND is_todo=1");
        query.bindValue(":dt", now);
        query.bindValue(":id", id);
        if (!query.exec()) {
            qWarning() << "批量恢复待办失败 id=" << id;
            return false;
        }
    }
    return true;
}

// ─── 查询方法 ──────────────────────────────────────────────────

TodoData TodoStorage::getTodo(int id) const
{
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM notes_todos WHERE id=:id");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        TodoData todo = rowToTodo(queryToMap(query));
        todo.tags = getTagsStringList(id);
        return todo;
    }
    return TodoData();
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
        while (query.next()) {
            TodoData todo = rowToTodo(queryToMap(query));
            todo.tags = getTagsStringList(todo.id);
            list.append(todo);
        }
    }
    return list;
}

QList<TodoData> TodoStorage::getPendingTodos(const TodoSortParam &sort) const
{
    QList<TodoData> list;
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM notes_todos WHERE is_todo=1 AND is_completed=0 AND is_deleted=0"
                  + buildTodoOrderClause(sort));
    if (query.exec()) {
        while (query.next()) {
            TodoData todo = rowToTodo(queryToMap(query));
            todo.tags = getTagsStringList(todo.id);
            list.append(todo);
        }
    }
    return list;
}

QList<TodoData> TodoStorage::getCompletedTodos() const
{
    QList<TodoData> list;
    QSqlQuery query(m_db->connection());
    if (query.exec("SELECT * FROM notes_todos WHERE is_todo=1 AND is_completed=1 AND is_deleted=0 ORDER BY completed_datetime DESC")) {
        while (query.next()) {
            TodoData todo = rowToTodo(queryToMap(query));
            todo.tags = getTagsStringList(todo.id);
            list.append(todo);
        }
    }
    return list;
}

QList<TodoData> TodoStorage::getDeletedTodos() const
{
    QList<TodoData> list;
    QSqlQuery query(m_db->connection());
    if (query.exec("SELECT * FROM notes_todos WHERE is_todo=1 AND is_deleted=1 ORDER BY deletion_datetime DESC")) {
        while (query.next()) {
            TodoData todo = rowToTodo(queryToMap(query));
            todo.tags = getTagsStringList(todo.id);
            list.append(todo);
        }
    }
    return list;
}

QList<TodoData> TodoStorage::getTodayTodos(const TodoSortParam &sort) const
{
    QList<TodoData> list;
    qint64 todayStart = QDateTime(QDate::currentDate(), QTime(0,0)).toSecsSinceEpoch();
    qint64 todayEnd = QDateTime(QDate::currentDate(), QTime(23,59,59)).toSecsSinceEpoch();
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM notes_todos WHERE is_todo=1 AND is_deleted=0 AND is_completed=0"
                  " AND ((due_datetime>=:start AND due_datetime<=:end) OR due_datetime=0)"
                  + buildTodoOrderClause(sort));
    query.bindValue(":start", todayStart);
    query.bindValue(":end", todayEnd);
    if (query.exec()) {
        while (query.next()) {
            TodoData todo = rowToTodo(queryToMap(query));
            todo.tags = getTagsStringList(todo.id);
            list.append(todo);
        }
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
        while (query.next()) {
            TodoData todo = rowToTodo(queryToMap(query));
            todo.tags = getTagsStringList(todo.id);
            list.append(todo);
        }
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
        while (query.next()) {
            TodoData todo = rowToTodo(queryToMap(query));
            todo.tags = getTagsStringList(todo.id);
            list.append(todo);
        }
    }
    return list;
}

QList<TodoData> TodoStorage::getTodosByTag(const QString &tag) const
{
    QList<TodoData> list;
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT DISTINCT n.* FROM notes_todos n "
                  "LEFT JOIN todo_tags tt ON n.id = tt.todo_id "
                  "LEFT JOIN tags t ON tt.tag_id = t.id "
                  "WHERE n.is_todo=1 AND n.is_deleted=0 AND (n.tag LIKE :tag LIKE :tag2 OR t.name = :tag3) "
                  "ORDER BY n.modification_datetime DESC");
    QString likePattern = "%" + tag + "%";
    query.bindValue(":tag", likePattern);
    query.bindValue(":tag2", likePattern);
    query.bindValue(":tag3", tag);
    if (query.exec()) {
        while (query.next()) {
            TodoData todo = rowToTodo(queryToMap(query));
            todo.tags = getTagsStringList(todo.id);
            list.append(todo);
        }
    }
    return list;
}

QList<TodoData> TodoStorage::getTodosByTags(const QStringList &tags) const
{
    if (tags.isEmpty()) return {};
    QList<TodoData> list;
    QSqlQuery query(m_db->connection());
    // 查找包含所有指定标签的待办（AND 逻辑）
    QString placeholders;
    QStringList bindVals;
    for (int i = 0; i < tags.size(); ++i) {
        if (i > 0) placeholders += ",";
        placeholders += QString(":tag%1").arg(i);
        bindVals << tags[i];
    }
    QString sql = QString(
        "SELECT n.* FROM notes_todos n "
        "WHERE n.is_todo=1 AND n.is_deleted=0 AND n.id IN ("
        "  SELECT tt.todo_id FROM todo_tags tt "
        "  JOIN tags t ON tt.tag_id = t.id "
        "  WHERE t.name IN (%1)"
        "  GROUP BY tt.todo_id HAVING COUNT(DISTINCT t.name) = %2"
        ")"
    ).arg(placeholders).arg(tags.size());

    // 也搜索旧的 tag 字段
    sql += " UNION SELECT n.* FROM notes_todos n WHERE n.is_todo=1 AND n.is_deleted=0 AND n.tag IN (";
    QString tagPlaceholders;
    for (int i = 0; i < tags.size(); ++i) {
        if (i > 0) tagPlaceholders += ",";
        tagPlaceholders += QString(":otag%1").arg(i);
    }
    sql += tagPlaceholders + ") ORDER BY modification_datetime DESC";

    query.prepare(sql);
    for (int i = 0; i < tags.size(); ++i) {
        query.bindValue(QString(":tag%1").arg(i), tags[i]);
        query.bindValue(QString(":otag%1").arg(i), tags[i]);
    }
    if (query.exec()) {
        QSet<int> seen;
        while (query.next()) {
            TodoData todo = rowToTodo(queryToMap(query));
            if (!seen.contains(todo.id)) {
                seen.insert(todo.id);
                todo.tags = getTagsStringList(todo.id);
                list.append(todo);
            }
        }
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
        while (query.next()) {
            TodoData todo = rowToTodo(queryToMap(query));
            todo.tags = getTagsStringList(todo.id);
            list.append(todo);
        }
    }
    return list;
}

QList<TagStat> TodoStorage::getTagStats(qint64 startSecs, qint64 endSecs) const
{
    QList<TagStat> result;
    QSqlQuery query(m_db->connection());
    // 生效日期 = due_datetime（无截止则用 creation_datetime），与周视图每日待办口径一致
    query.prepare(R"(
        SELECT COALESCE(t.name, NULLIF(n.tag, ''), :untagged) AS tag_name,
               COUNT(DISTINCT n.id) AS total,
               COALESCE(SUM(CASE WHEN n.is_completed = 1 THEN 1 ELSE 0 END), 0) AS completed
        FROM notes_todos n
        LEFT JOIN todo_tags tt ON tt.todo_id = n.id
        LEFT JOIN tags t ON tt.tag_id = t.id
        WHERE n.is_todo = 1 AND n.is_deleted = 0
          AND (CASE WHEN n.due_datetime > 0 THEN n.due_datetime ELSE n.creation_datetime END)
              BETWEEN :start AND :end
        GROUP BY tag_name
        ORDER BY total DESC, tag_name ASC
    )");
    query.bindValue(":start", startSecs);
    query.bindValue(":end", endSecs);
    query.bindValue(":untagged", QStringLiteral("未分类"));
    if (query.exec()) {
        while (query.next()) {
            TagStat stat;
            stat.tag = query.value("tag_name").toString();
            stat.total = query.value("total").toInt();
            stat.completed = query.value("completed").toInt();
            result.append(stat);
        }
    }
    return result;
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

// ─── 多标签方法 ────────────────────────────────────────────────

QStringList TodoStorage::getTodoTags(int id) const
{
    return getTagsStringList(id);
}

bool TodoStorage::setTodoTags(int id, const QStringList &tags)
{
    // 先清除旧关联
    QSqlQuery clearQuery(m_db->connection());
    clearQuery.prepare("DELETE FROM todo_tags WHERE todo_id=:id");
    clearQuery.bindValue(":id", id);
    if (!clearQuery.exec()) {
        qWarning() << "清除待办标签关联失败:" << clearQuery.lastError().text();
        return false;
    }

    if (tags.isEmpty()) return true;

    // 建立新关联（自动创建不存在的标签）
    for (const QString &tagName : tags) {
        if (tagName.trimmed().isEmpty()) continue;

        // 查找或创建标签
        QSqlQuery findTag(m_db->connection());
        findTag.prepare("SELECT id FROM tags WHERE name=:name");
        findTag.bindValue(":name", tagName.trimmed());
        int tagId = -1;
        if (findTag.exec() && findTag.next()) {
            tagId = findTag.value(0).toInt();
        } else {
            // 自动创建标签
            QSqlQuery createTag(m_db->connection());
            createTag.prepare("INSERT INTO tags (name, color, created_at) VALUES (:name, :color, :now)");
            createTag.bindValue(":name", tagName.trimmed());
            // 根据标签名哈希选一个颜色
            QStringList colors = {"#1890FF", "#52C41A", "#FAAD14", "#722ED1", "#13C2C2", "#EB2F96", "#FA8C16"};
            int colorIdx = qHash(tagName.trimmed()) % colors.size();
            createTag.bindValue(":color", colors[colorIdx]);
            createTag.bindValue(":now", QDateTime::currentSecsSinceEpoch());
            if (createTag.exec()) {
                tagId = createTag.lastInsertId().toInt();
            }
        }

        if (tagId > 0) {
            QSqlQuery insertRel(m_db->connection());
            insertRel.prepare("INSERT OR IGNORE INTO todo_tags (todo_id, tag_id) VALUES (:todo_id, :tag_id)");
            insertRel.bindValue(":todo_id", id);
            insertRel.bindValue(":tag_id", tagId);
            if (!insertRel.exec()) {
                qWarning() << "插入待办标签关联失败:" << insertRel.lastError().text();
            }
        }
    }

    // 同步旧 tag 字段：取第一个标签
    QSqlQuery updateOld(m_db->connection());
    updateOld.prepare("UPDATE notes_todos SET tag=:tag WHERE id=:id");
    updateOld.bindValue(":tag", tags.isEmpty() ? "" : tags.first());
    updateOld.bindValue(":id", id);
    updateOld.exec();

    return true;
}

bool TodoStorage::addTodoTag(int id, int tagId)
{
    QSqlQuery query(m_db->connection());
    query.prepare("INSERT OR IGNORE INTO todo_tags (todo_id, tag_id) VALUES (:todo_id, :tag_id)");
    query.bindValue(":todo_id", id);
    query.bindValue(":tag_id", tagId);
    if (!query.exec()) {
        qWarning() << "添加待办标签关联失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool TodoStorage::removeTodoTag(int id, int tagId)
{
    QSqlQuery query(m_db->connection());
    query.prepare("DELETE FROM todo_tags WHERE todo_id=:todo_id AND tag_id=:tag_id");
    query.bindValue(":todo_id", id);
    query.bindValue(":tag_id", tagId);
    if (!query.exec()) {
        qWarning() << "移除待办标签关联失败:" << query.lastError().text();
        return false;
    }
    return true;
}

QStringList TodoStorage::getTagsStringList(int id) const
{
    QStringList result;
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT t.name FROM todo_tags tt JOIN tags t ON tt.tag_id = t.id WHERE tt.todo_id=:id ORDER BY t.name");
    query.bindValue(":id", id);
    if (query.exec()) {
        while (query.next()) {
            result.append(query.value(0).toString());
        }
    }
    // 如果 todo_tags 为空但旧 tag 字段有值，用旧字段补
    if (result.isEmpty()) {
        QSqlQuery oldQuery(m_db->connection());
        oldQuery.prepare("SELECT tag FROM notes_todos WHERE id=:id");
        oldQuery.bindValue(":id", id);
        if (oldQuery.exec() && oldQuery.next()) {
            QString oldTag = oldQuery.value(0).toString();
            if (!oldTag.isEmpty()) {
                result.append(oldTag);
            }
        }
    }
    return result;
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

QString TodoStorage::buildTodoOrderClause(const TodoSortParam &sort) const
{
    QString field;
    switch (sort.field) {
    case TodoSortParam::DueDate:   field = "due_datetime"; break;
    case TodoSortParam::CreatedAt: field = "creation_datetime"; break;
    default: field = "due_datetime"; break;
    }
    return QString(" ORDER BY %1 %2").arg(field, sort.ascending ? "ASC" : "DESC");
}
