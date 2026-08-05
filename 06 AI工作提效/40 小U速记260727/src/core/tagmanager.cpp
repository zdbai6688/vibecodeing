// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tagmanager.h"
#include "storage/database.h"

TagManager::TagManager(Database *db, QObject *parent)
    : QObject(parent)
{
    m_storage = new TagStorage(db, this);
}

int TagManager::createTag(const QString &name, const QString &color)
{
    int id = m_storage->createTag(name, color);
    if (id > 0) {
        emit tagCreated(id);
        emit dataChanged();
    }
    return id;
}

bool TagManager::updateTag(int id, const QString &name, const QString &color)
{
    bool ok = m_storage->updateTag(id, name, color);
    if (ok) {
        emit tagUpdated(id);
        emit dataChanged();
    }
    return ok;
}

bool TagManager::deleteTag(int id)
{
    bool ok = m_storage->deleteTag(id);
    if (ok) {
        emit tagDeleted(id);
        emit dataChanged();
    }
    return ok;
}

TagData TagManager::getTag(int id) const { return m_storage->getTag(id); }
TagData TagManager::getTagByName(const QString &name) const { return m_storage->getTagByName(name); }
QList<TagData> TagManager::getAllTags() const { return m_storage->getAllTags(); }
QStringList TagManager::allTagNames() const { return m_storage->allTagNames(); }