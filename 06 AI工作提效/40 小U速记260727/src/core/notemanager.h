// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NOTEMANAGER_H
#define NOTEMANAGER_H

#include <QObject>
#include "storage/notestorage.h"

class Database;

class NoteManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(NoteManager)

public:
    explicit NoteManager(Database *db, QObject *parent = nullptr);

    // CRUD 委托
    int createNote(const NoteData &note);
    bool updateNote(const NoteData &note);
    bool deleteNote(int id);
    bool restoreNote(int id);
    bool permanentDelete(int id);
    bool permanentDeleteAll();

    // 批量操作
    bool batchDeleteNotes(const QList<int> &ids);
    bool batchRestoreNotes(const QList<int> &ids);
    bool batchPermanentDelete(const QList<int> &ids);

    NoteData getNote(int id) const;
    QList<NoteData> getAllNotes(const NoteSortParam &sort = NoteSortParam()) const;
    QList<NoteData> getDeletedNotes() const;
    QList<NoteData> searchDeletedNotes(const QString &keyword, const NoteSortParam &sort = NoteSortParam()) const;
    QList<NoteData> searchNotes(const QString &keyword, const NoteSortParam &sort = NoteSortParam()) const;
    QList<NoteData> getNotesByTag(const QString &tag, const NoteSortParam &sort = NoteSortParam()) const;
    QList<NoteData> getNotesByFolder(int folderId, const NoteSortParam &sort = NoteSortParam()) const;

    int noteCount() const;

    bool convertToTodo(int id, int priority = 0, qint64 dueDatetime = 0);
    bool convertToNote(int id);

signals:
    void noteCreated(int id);
    void noteUpdated(int id);
    void noteDeleted(int id);
    void noteRestored(int id);
    void dataChanged();

private:
    NoteStorage *m_storage;
};

#endif // NOTEMANAGER_H