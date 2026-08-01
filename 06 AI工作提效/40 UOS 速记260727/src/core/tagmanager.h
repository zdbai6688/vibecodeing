// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TAGMANAGER_H
#define TAGMANAGER_H

#include <QObject>
#include "storage/tagstorage.h"

class Database;

class TagManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(TagManager)

public:
    explicit TagManager(Database *db, QObject *parent = nullptr);

    int createTag(const QString &name, const QString &color = "#1890FF");
    bool updateTag(int id, const QString &name, const QString &color);
    bool deleteTag(int id);

    TagData getTag(int id) const;
    TagData getTagByName(const QString &name) const;
    QList<TagData> getAllTags() const;
    QStringList allTagNames() const;

signals:
    void tagCreated(int id);
    void tagUpdated(int id);
    void tagDeleted(int id);
    void dataChanged();

private:
    TagStorage *m_storage;
};

#endif // TAGMANAGER_H