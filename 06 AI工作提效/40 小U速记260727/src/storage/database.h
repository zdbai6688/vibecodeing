// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>

class Database : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Database)

public:
    explicit Database(QObject *parent = nullptr);
    ~Database() override;

    bool initialize();
    virtual QSqlDatabase &connection(); // virtual 以便测试子类注入内存连接

    // 数据路径
    virtual QString dataPath() const { return m_dataPath; }
    void setDataPath(const QString &path) { m_dataPath = path; }

private:
    bool createTables();
    QString m_dataPath;
    QSqlDatabase m_db;
};

#endif // DATABASE_H