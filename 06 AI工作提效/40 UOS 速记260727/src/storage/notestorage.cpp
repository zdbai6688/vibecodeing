// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "notestorage.h"
#include "storage/database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include <QJsonDocument>

QString NoteData::previewText(int maxLen) const
{
    // 去除Markdown标记，取纯文本前maxLen字符
    QString plain = content;
    plain.remove(QRegularExpression("[#*`\\[\\]]"));
    plain.replace(QRegularExpression("!\\[.*\\]\\(.*\\)"), "[图片]");
    plain.replace(QRegularExpression("\\[.*\\]\\(.*\\)"), "\\1");
    if (plain.length() > maxLen) {
        plain = plain.left(maxLen) + "...";
    }
    return plain;
}

NoteStorage::NoteStorage(Database *db, QObject *parent)
    : QObject(parent), m_db(db)
{
}

int NoteStorage::createNote(const NoteData &note)
{
    QSqlQuery query(m_db->connection());
    query.prepare(R"(
        INSERT INTO notes_todos
            (title, content, content_type, tag, folder_id,
             creation_datetime, modification_datetime, is_deleted)
        VALUES
            (:title, :content, :content_type, :tag, :folder_id,
             :creation, :modification, :deleted)
    )");

    qint64 now = QDateTime::currentSecsSinceEpoch();
    query.bindValue(":title", note.title);
    query.bindValue(":content", note.content);
    query.bindValue(":content_type", note.contentType);
    query.bindValue(":tag", note.tag);
    query.bindValue(":folder_id", note.folderId);
    query.bindValue(":creation", now);
    query.bindValue(":modification", now);
    query.bindValue(":deleted", 0);

    if (!query.exec()) {
        qWarning() << "创建笔记失败:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

bool NoteStorage::updateNote(const NoteData &note)
{
    QSqlQuery query(m_db->connection());
    query.prepare(R"(
        UPDATE notes_todos SET
            title = :title,
            content = :content,
            content_type = :content_type,
            tag = :tag,
            folder_id = :folder_id,
            modification_datetime = :modification
        WHERE id = :id
    )");

    query.bindValue(":title", note.title);
    query.bindValue(":content", note.content);
    query.bindValue(":content_type", note.contentType);
    query.bindValue(":tag", note.tag);
    query.bindValue(":folder_id", note.folderId);
    query.bindValue(":modification", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", note.id);

    if (!query.exec()) {
        qWarning() << "更新笔记失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool NoteStorage::deleteNote(int id)
{
    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE notes_todos SET is_deleted = 1, deletion_datetime = :del WHERE id = :id");
    query.bindValue(":del", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", id);
    return query.exec();
}

bool NoteStorage::restoreNote(int id)
{
    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE notes_todos SET is_deleted = 0, deletion_datetime = 0 WHERE id = :id");
    query.bindValue(":id", id);
    return query.exec();
}

bool NoteStorage::permanentDelete(int id)
{
    QSqlQuery query(m_db->connection());
    query.prepare("DELETE FROM notes_todos WHERE id = :id AND is_deleted = 1");
    query.bindValue(":id", id);
    return query.exec();
}

bool NoteStorage::permanentDeleteAll()
{
    QSqlQuery query(m_db->connection());
    return query.exec("DELETE FROM notes_todos WHERE is_deleted = 1");
}

NoteData NoteStorage::getNote(int id) const
{
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM notes_todos WHERE id = :id");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        QVariantMap row;
        for (int i = 0; i < query.record().count(); ++i) {
            row[query.record().fieldName(i)] = query.value(i);
        }
        return rowToNote(row);
    }
    return NoteData();
}

QList<NoteData> NoteStorage::getAllNotes(bool includeDeleted) const
{
    QList<NoteData> result;
    QSqlQuery query(m_db->connection());
    QString sql = "SELECT * FROM notes_todos WHERE is_todo = 0";
    if (!includeDeleted) {
        sql += " AND is_deleted = 0";
    }
    sql += " ORDER BY modification_datetime DESC";

    if (query.exec(sql)) {
        while (query.next()) {
            QVariantMap row;
            for (int i = 0; i < query.record().count(); ++i) {
                row[query.record().fieldName(i)] = query.value(i);
            }
            result.append(rowToNote(row));
        }
    }
    return result;
}

QList<NoteData> NoteStorage::getDeletedNotes() const
{
    QList<NoteData> result;
    QSqlQuery query(m_db->connection());
    if (query.exec("SELECT * FROM notes_todos WHERE is_deleted = 1 ORDER BY deletion_datetime DESC")) {
        while (query.next()) {
            QVariantMap row;
            for (int i = 0; i < query.record().count(); ++i) {
                row[query.record().fieldName(i)] = query.value(i);
            }
            result.append(rowToNote(row));
        }
    }
    return result;
}

QList<NoteData> NoteStorage::searchNotes(const QString &keyword) const
{
    QList<NoteData> result;
    QSqlQuery query(m_db->connection());
    query.prepare(R"(
        SELECT * FROM notes_todos
        WHERE is_deleted = 0 AND is_todo = 0
        AND (title LIKE :kw OR content LIKE :kw2)
        ORDER BY modification_datetime DESC
    )");
    QString like = "%" + keyword + "%";
    query.bindValue(":kw", like);
    query.bindValue(":kw2", like);

    if (query.exec()) {
        while (query.next()) {
            QVariantMap row;
            for (int i = 0; i < query.record().count(); ++i) {
                row[query.record().fieldName(i)] = query.value(i);
            }
            result.append(rowToNote(row));
        }
    }
    return result;
}

QList<NoteData> NoteStorage::getNotesByTag(const QString &tag) const
{
    QList<NoteData> result;
    QSqlQuery query(m_db->connection());
    query.prepare(R"(
        SELECT * FROM notes_todos
        WHERE is_deleted = 0 AND is_todo = 0
        AND tag LIKE :tag
        ORDER BY modification_datetime DESC
    )");
    query.bindValue(":tag", "%" + tag + "%");
    if (query.exec()) {
        while (query.next()) {
            QVariantMap row;
            for (int i = 0; i < query.record().count(); ++i) {
                row[query.record().fieldName(i)] = query.value(i);
            }
            result.append(rowToNote(row));
        }
    }
    return result;
}

QList<NoteData> NoteStorage::getNotesByFolder(int folderId) const
{
    QList<NoteData> result;
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM notes_todos WHERE is_deleted = 0 AND is_todo = 0 AND folder_id = :fid ORDER BY modification_datetime DESC");
    query.bindValue(":fid", folderId);
    if (query.exec()) {
        while (query.next()) {
            QVariantMap row;
            for (int i = 0; i < query.record().count(); ++i) {
                row[query.record().fieldName(i)] = query.value(i);
            }
            result.append(rowToNote(row));
        }
    }
    return result;
}

bool NoteStorage::convertToTodo(int id, int priority, qint64 dueDatetime)
{
    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE notes_todos SET is_todo=1, priority=:p, due_datetime=:due, modification_datetime=:m WHERE id=:id");
    query.bindValue(":p", priority);
    query.bindValue(":due", dueDatetime);
    query.bindValue(":m", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", id);
    return query.exec();
}

bool NoteStorage::convertToNote(int id)
{
    QSqlQuery query(m_db->connection());
    query.prepare("UPDATE notes_todos SET is_todo=0, priority=0, due_datetime=0, is_completed=0, completed_datetime=0, modification_datetime=:m WHERE id=:id");
    query.bindValue(":m", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", id);
    return query.exec();
}

int NoteStorage::noteCount() const
{
    QSqlQuery query(m_db->connection());
    query.exec("SELECT COUNT(*) FROM notes_todos WHERE is_deleted = 0 AND is_todo = 0");
    if (query.next()) return query.value(0).toInt();
    return 0;
}

NoteData NoteStorage::rowToNote(const QVariantMap &row) const
{
    NoteData note;
    note.id = row["id"].toInt();
    note.title = row["title"].toString();
    note.content = row["content"].toString();
    note.contentType = row["content_type"].toString();
    note.tag = row["tag"].toString();
    note.folderId = row["folder_id"].toInt();
    note.creationDatetime = row["creation_datetime"].toLongLong();
    note.modificationDatetime = row["modification_datetime"].toLongLong();
    note.isDeleted = row["is_deleted"].toBool();
    note.deletionDatetime = row["deletion_datetime"].toLongLong();
    return note;
}