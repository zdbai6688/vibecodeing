// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "todomanager.h"
#include "storage/database.h"

TodoManager::TodoManager(Database *db, QObject *parent)
    : QObject(parent)
{
    m_storage = new TodoStorage(db, this);
}

int TodoManager::createTodo(const TodoData &todo)
{
    int id = m_storage->createTodo(todo);
    if (id > 0) {
        emit todoCreated(id);
        emit dataChanged();
    }
    return id;
}

bool TodoManager::updateTodo(const TodoData &todo)
{
    bool ok = m_storage->updateTodo(todo);
    if (ok) {
        emit todoUpdated(todo.id);
        emit dataChanged();
    }
    return ok;
}

bool TodoManager::toggleComplete(int id, bool completed)
{
    bool ok = m_storage->toggleComplete(id, completed);
    if (ok) {
        emit todoToggled(id, completed);
        emit dataChanged();
    }
    return ok;
}

bool TodoManager::setPriority(int id, int priority)
{
    bool ok = m_storage->setPriority(id, priority);
    if (ok) {
        emit todoUpdated(id);
        emit dataChanged();
    }
    return ok;
}

bool TodoManager::setTag(int id, const QString &tag)
{
    bool ok = m_storage->setTag(id, tag);
    if (ok) {
        emit todoUpdated(id);
        emit dataChanged();
    }
    return ok;
}

bool TodoManager::deleteTodo(int id)
{
    bool ok = m_storage->deleteTodo(id);
    if (ok) {
        emit todoDeleted(id);
        emit dataChanged();
    }
    return ok;
}

bool TodoManager::restoreTodo(int id)
{
    bool ok = m_storage->restoreTodo(id);
    if (ok) {
        emit todoRestored(id);
        emit dataChanged();
    }
    return ok;
}

bool TodoManager::permanentDelete(int id)
{
    bool ok = m_storage->permanentDelete(id);
    if (ok) emit dataChanged();
    return ok;
}

// ─── 批量操作 ─────────────────────────────────────────────────────

bool TodoManager::batchDeleteTodos(const QList<int> &ids)
{
    bool ok = m_storage->batchDeleteTodos(ids);
    if (ok) {
        for (int id : ids) {
            emit todoDeleted(id);
        }
        emit dataChanged();
    }
    return ok;
}

bool TodoManager::batchRestoreTodos(const QList<int> &ids)
{
    bool ok = m_storage->batchRestoreTodos(ids);
    if (ok) {
        for (int id : ids) {
            emit todoRestored(id);
        }
        emit dataChanged();
    }
    return ok;
}

// ─── 查询方法 ─────────────────────────────────────────────────────

TodoData TodoManager::getTodo(int id) const { return m_storage->getTodo(id); }
QList<TodoData> TodoManager::getAllTodos() const { return m_storage->getAllTodos(false, false); }
QList<TodoData> TodoManager::getPendingTodos(const TodoSortParam &sort) const { return m_storage->getPendingTodos(sort); }
QList<TodoData> TodoManager::getCompletedTodos() const { return m_storage->getCompletedTodos(); }
QList<TodoData> TodoManager::getDeletedTodos() const { return m_storage->getDeletedTodos(); }
QList<TodoData> TodoManager::getTodayTodos(const TodoSortParam &sort) const { return m_storage->getTodayTodos(sort); }
QList<TodoData> TodoManager::getOverdueTodos(const TodoSortParam &sort) const { return m_storage->getOverdueTodos(sort); }
QList<TodoData> TodoManager::getWeekTodos(const TodoSortParam &sort) const { return m_storage->getWeekTodos(sort); }
QList<TodoData> TodoManager::getTodosByTag(const QString &tag) const { return m_storage->getTodosByTag(tag); }
QList<TodoData> TodoManager::getTodosByTags(const QStringList &tags) const { return m_storage->getTodosByTags(tags); }
QList<TodoData> TodoManager::searchTodos(const QString &keyword) const { return m_storage->searchTodos(keyword); }
int TodoManager::pendingCount() const { return m_storage->pendingCount(); }
int TodoManager::completedCount() const { return m_storage->completedCount(); }

// ─── 多标签方法 ─────────────────────────────────────────────────────

QStringList TodoManager::getTodoTags(int id) const { return m_storage->getTodoTags(id); }

bool TodoManager::setTodoTags(int id, const QStringList &tags)
{
    bool ok = m_storage->setTodoTags(id, tags);
    if (ok) {
        emit todoUpdated(id);
        emit dataChanged();
    }
    return ok;
}

bool TodoManager::addTodoTag(int id, int tagId)
{
    bool ok = m_storage->addTodoTag(id, tagId);
    if (ok) {
        emit todoUpdated(id);
        emit dataChanged();
    }
    return ok;
}

bool TodoManager::removeTodoTag(int id, int tagId)
{
    bool ok = m_storage->removeTodoTag(id, tagId);
    if (ok) {
        emit todoUpdated(id);
        emit dataChanged();
    }
    return ok;
}
