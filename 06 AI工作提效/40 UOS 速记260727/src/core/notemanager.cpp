// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "notemanager.h"
#include "storage/database.h"

NoteManager::NoteManager(Database *db, QObject *parent)
    : QObject(parent)
{
    m_storage = new NoteStorage(db, this);
}

int NoteManager::createNote(const NoteData &note)
{
    int id = m_storage->createNote(note);
    if (id > 0) {
        emit noteCreated(id);
        emit dataChanged();
    }
    return id;
}

bool NoteManager::updateNote(const NoteData &note)
{
    bool ok = m_storage->updateNote(note);
    if (ok) {
        emit noteUpdated(note.id);
        emit dataChanged();
    }
    return ok;
}

bool NoteManager::deleteNote(int id)
{
    bool ok = m_storage->deleteNote(id);
    if (ok) {
        emit noteDeleted(id);
        emit dataChanged();
    }
    return ok;
}

bool NoteManager::restoreNote(int id)
{
    bool ok = m_storage->restoreNote(id);
    if (ok) {
        emit noteRestored(id);
        emit dataChanged();
    }
    return ok;
}

bool NoteManager::permanentDelete(int id)
{
    bool ok = m_storage->permanentDelete(id);
    if (ok) emit dataChanged();
    return ok;
}

bool NoteManager::permanentDeleteAll()
{
    bool ok = m_storage->permanentDeleteAll();
    if (ok) emit dataChanged();
    return ok;
}

NoteData NoteManager::getNote(int id) const { return m_storage->getNote(id); }
QList<NoteData> NoteManager::getAllNotes() const { return m_storage->getAllNotes(false); }
QList<NoteData> NoteManager::getDeletedNotes() const { return m_storage->getDeletedNotes(); }
QList<NoteData> NoteManager::searchNotes(const QString &keyword) const { return m_storage->searchNotes(keyword); }
QList<NoteData> NoteManager::getNotesByTag(const QString &tag) const { return m_storage->getNotesByTag(tag); }
QList<NoteData> NoteManager::getNotesByFolder(int folderId) const { return m_storage->getNotesByFolder(folderId); }
int NoteManager::noteCount() const { return m_storage->noteCount(); }

bool NoteManager::convertToTodo(int id, int priority, qint64 dueDatetime)
{
    bool ok = m_storage->convertToTodo(id, priority, dueDatetime);
    if (ok) { emit dataChanged(); }
    return ok;
}

bool NoteManager::convertToNote(int id)
{
    bool ok = m_storage->convertToNote(id);
    if (ok) { emit dataChanged(); }
    return ok;
}