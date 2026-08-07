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

    // 关闭并释放当前连接（供备份恢复等场景在 removeDatabase 前调用，
    // 避免 QSqlDatabase 值对象仍持有引用触发 "still in use" QWARN）
    virtual void closeConnection();

    // 数据路径
    virtual QString dataPath() const { return m_dataPath; }
    void setDataPath(const QString &path) { m_dataPath = path; }

private:
    bool createTables();
    QString m_dataPath;
    QSqlDatabase m_db;
};

#endif // DATABASE_H