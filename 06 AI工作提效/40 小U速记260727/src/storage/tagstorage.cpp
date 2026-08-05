// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tagstorage.h"
#include "storage/database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>
#include <QDebug>

TagStorage::TagStorage(Database *db, QObject *parent)
    : QObject(parent), m_db(db)
{
}

int TagStorage::createTag(const QString &name, const QString &color)
{
    QSqlQuery query(m_db->connection());
    query.prepare("INSERT INTO tags (name, color, created_at) VALUES (?, ?, ?)");
    query.addBindValue(name);
    query.addBindValue(color);
    query.addBindValue(QDateTime::currentSecsSinceEpoch());

    if (!query.exec()) {
        qWarning() << "创建标签失败:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

bool TagStorage::updateTag(int id, const QString &name, const QString &color)
{
    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE tags SET name=?, color=? WHERE id=?");
    query.addBindValue(name);
    query.addBindValue(color);
    query.addBindValue(id);

    if (!query.exec()) {
        qWarning() << "更新标签失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool TagStorage::deleteTag(int id)
{
    QSqlQuery query(m_db->connection());
    query.prepare("DELETE FROM tags WHERE id=?");
    query.addBindValue(id);

    if (!query.exec()) {
        qWarning() << "删除标签失败:" << query.lastError().text();
        return false;
    }
    return true;
}

TagData TagStorage::getTag(int id) const
{
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM tags WHERE id=?");
    query.addBindValue(id);

    if (query.exec() && query.next()) {
        QVariantMap row;
        for (int i = 0; i < query.record().count(); ++i) {
            row[query.record().fieldName(i)] = query.value(i);
        }
        return rowToTag(row);
    }
    return TagData();
}

TagData TagStorage::getTagByName(const QString &name) const
{
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM tags WHERE name=?");
    query.addBindValue(name);

    if (query.exec() && query.next()) {
        QVariantMap row;
        for (int i = 0; i < query.record().count(); ++i) {
            row[query.record().fieldName(i)] = query.value(i);
        }
        return rowToTag(row);
    }
    return TagData();
}

QList<TagData> TagStorage::getAllTags() const
{
    QList<TagData> result;
    QSqlQuery query(m_db->connection());
    query.exec("SELECT * FROM tags ORDER BY id ASC");

    while (query.next()) {
        QVariantMap row;
        for (int i = 0; i < query.record().count(); ++i) {
            row[query.record().fieldName(i)] = query.value(i);
        }
        result.append(rowToTag(row));
    }
    return result;
}

QStringList TagStorage::allTagNames() const
{
    QStringList names;
    QSqlQuery query(m_db->connection());
    query.exec("SELECT name FROM tags ORDER BY id ASC");

    while (query.next()) {
        names.append(query.value(0).toString());
    }
    return names;
}

TagData TagStorage::rowToTag(const QVariantMap &row) const
{
    TagData tag;
    tag.id = row["id"].toInt();
    tag.name = row["name"].toString();
    tag.color = row["color"].toString();
    tag.createdAt = row["created_at"].toLongLong();
    return tag;
}