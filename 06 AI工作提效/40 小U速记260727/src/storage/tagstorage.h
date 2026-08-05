// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TAGSTORAGE_H
#define TAGSTORAGE_H

#include <QObject>
#include <QList>
#include <QString>

class Database;

struct TagData {
    int id = 0;
    QString name;
    QString color = "#1890FF";
    qint64 createdAt = 0;
};

class TagStorage : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(TagStorage)

public:
    explicit TagStorage(Database *db, QObject *parent = nullptr);

    int createTag(const QString &name, const QString &color = "#1890FF");
    bool updateTag(int id, const QString &name, const QString &color);
    bool deleteTag(int id);

    TagData getTag(int id) const;
    TagData getTagByName(const QString &name) const;
    QList<TagData> getAllTags() const;
    QStringList allTagNames() const;

private:
    TagData rowToTag(const QVariantMap &row) const;
    Database *m_db;
};

#endif // TAGSTORAGE_H