// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NOTESTORAGE_H
#define NOTESTORAGE_H

#include <QObject>
#include <QList>
#include <QDateTime>
#include <QVariantMap>

class Database;

struct NoteData {
    int id = 0;
    QString title;
    QString content;
    QString contentType = "markdown"; // markdown / richtext
    QString tag;
    int folderId = 0;
    qint64 creationDatetime = 0;
    qint64 modificationDatetime = 0;
    bool isDeleted = false;
    qint64 deletionDatetime = 0;

    // 便捷方法
    QDateTime createdAt() const { return QDateTime::fromSecsSinceEpoch(creationDatetime); }
    QDateTime modifiedAt() const { return QDateTime::fromSecsSinceEpoch(modificationDatetime); }
    QString previewText(int maxLen = 100) const;
};

// 笔记排序参数
struct NoteSortParam {
    enum Field { ModifiedAt = 0, CreatedAt };
    Field field = ModifiedAt;
    bool ascending = false; // false = DESC（默认：修改时间倒序）
};

class NoteStorage : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(NoteStorage)

public:
    explicit NoteStorage(Database *db, QObject *parent = nullptr);

    // CRUD
    int createNote(const NoteData &note);
    bool updateNote(const NoteData &note);
    bool deleteNote(int id);          // 软删除
    bool restoreNote(int id);         // 恢复
    bool permanentDelete(int id);     // 永久删除
    bool permanentDeleteAll();        // 清空回收站

    // 批量操作
    bool batchDeleteNotes(const QList<int> &ids);     // 批量软删除
    bool batchRestoreNotes(const QList<int> &ids);    // 批量恢复
    bool batchPermanentDelete(const QList<int> &ids); // 批量永久删除

    // 查询
    NoteData getNote(int id) const;
    QList<NoteData> getAllNotes(bool includeDeleted = false, const NoteSortParam &sort = NoteSortParam()) const;
    QList<NoteData> getDeletedNotes() const;
    QList<NoteData> searchDeletedNotes(const QString &keyword, const NoteSortParam &sort = NoteSortParam()) const; // 回收站内搜索
    QList<NoteData> searchNotes(const QString &keyword, const NoteSortParam &sort = NoteSortParam()) const;
    QList<NoteData> getNotesByTag(const QString &tag, const NoteSortParam &sort = NoteSortParam()) const;
    QList<NoteData> getNotesByFolder(int folderId, const NoteSortParam &sort = NoteSortParam()) const;

    int noteCount() const;

    bool convertToTodo(int id, int priority = 0, qint64 dueDatetime = 0);
    bool convertToNote(int id);

private:
    NoteData rowToNote(const QVariantMap &row) const;
    QString buildNoteOrderClause(const NoteSortParam &sort) const;
    Database *m_db;
};

#endif // NOTESTORAGE_H
