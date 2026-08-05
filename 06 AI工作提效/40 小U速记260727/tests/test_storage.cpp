#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

// 最小化数据库初始化和表创建
static bool createTestTables(QSqlDatabase &db)
{
    QSqlQuery query(db);
    const QString notesSql = R"(
        CREATE TABLE IF NOT EXISTS notes_todos (
            id                    INTEGER PRIMARY KEY AUTOINCREMENT,
            title                 TEXT NOT NULL DEFAULT '',
            content               TEXT DEFAULT '',
            content_type          TEXT DEFAULT 'markdown',
            is_todo               INTEGER DEFAULT 0,
            priority              INTEGER DEFAULT 0,
            is_completed          INTEGER DEFAULT 0,
            tag                   TEXT DEFAULT '',
            folder_id             INTEGER DEFAULT 0,
            creation_datetime     INTEGER NOT NULL,
            modification_datetime INTEGER NOT NULL,
            due_datetime          INTEGER DEFAULT 0,
            completed_datetime    INTEGER DEFAULT 0,
            is_deleted            INTEGER DEFAULT 0,
            deletion_datetime     INTEGER DEFAULT 0,
            sync_status           INTEGER DEFAULT 0
        )
    )";
    if (!query.exec(notesSql)) return false;

    const QString tagsSql = R"(
        CREATE TABLE IF NOT EXISTS tags (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT NOT NULL UNIQUE,
            color       TEXT DEFAULT '#1890FF',
            created_at  INTEGER NOT NULL
        )
    )";
    return query.exec(tagsSql);
}

class TestStorage : public QObject
{
    Q_OBJECT

private:
    QSqlDatabase m_db;

private slots:
    void initTestCase()
    {
        m_db = QSqlDatabase::addDatabase("QSQLITE", "test_connection");
        m_db.setDatabaseName(":memory:");
        QVERIFY(m_db.open());
        QVERIFY(createTestTables(m_db));
    }

    void cleanupTestCase()
    {
        m_db.close();
        // 释放 m_db 句柄: 连接仍被成员引用时调用 removeDatabase 会产生 QWARN
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase("test_connection");
    }

    // 笔记 CRUD 测试
    void testCreateNote()
    {
        QSqlQuery query(m_db);
        qint64 now = QDateTime::currentSecsSinceEpoch();
        query.prepare("INSERT INTO notes_todos (title, content, content_type, creation_datetime, modification_datetime) "
                       "VALUES (:title, :content, :type, :c, :m)");
        query.bindValue(":title", QString("测试笔记"));
        query.bindValue(":content", QString("测试内容"));
        query.bindValue(":type", QString("markdown"));
        query.bindValue(":c", now);
        query.bindValue(":m", now);
        QVERIFY(query.exec());

        int id = query.lastInsertId().toInt();
        QVERIFY(id > 0);

        // 验证
        query.prepare("SELECT * FROM notes_todos WHERE id = :id");
        query.bindValue(":id", id);
        QVERIFY(query.exec());
        QVERIFY(query.next());
        QCOMPARE(query.value("title").toString(), QString("测试笔记"));
        QCOMPARE(query.value("content").toString(), QString("测试内容"));
        QCOMPARE(query.value("is_todo").toInt(), 0);
        QCOMPARE(query.value("is_deleted").toInt(), 0);
    }

    void testUpdateNote()
    {
        QSqlQuery query(m_db);
        qint64 now = QDateTime::currentSecsSinceEpoch();
        query.prepare("INSERT INTO notes_todos (title, content, content_type, creation_datetime, modification_datetime) "
                       "VALUES (:title, :content, :type, :c, :m)");
        query.bindValue(":title", QString("更新前"));
        query.bindValue(":content", QString("旧内容"));
        query.bindValue(":type", QString("markdown"));
        query.bindValue(":c", now);
        query.bindValue(":m", now);
        QVERIFY(query.exec());
        int id = query.lastInsertId().toInt();

        query.prepare("UPDATE notes_todos SET title=:t, content=:c, modification_datetime=:m WHERE id=:id");
        query.bindValue(":t", QString("更新后"));
        query.bindValue(":c", QString("新内容"));
        query.bindValue(":m", QDateTime::currentSecsSinceEpoch());
        query.bindValue(":id", id);
        QVERIFY(query.exec());

        query.prepare("SELECT * FROM notes_todos WHERE id=:id");
        query.bindValue(":id", id);
        QVERIFY(query.exec());
        QVERIFY(query.next());
        QCOMPARE(query.value("title").toString(), QString("更新后"));
        QCOMPARE(query.value("content").toString(), QString("新内容"));
    }

    void testSoftDeleteAndRestore()
    {
        QSqlQuery query(m_db);
        qint64 now = QDateTime::currentSecsSinceEpoch();
        query.prepare("INSERT INTO notes_todos (title, content, creation_datetime, modification_datetime) "
                       "VALUES (:title, :content, :c, :m)");
        query.bindValue(":title", QString("待删除"));
        query.bindValue(":content", QString(""));
        query.bindValue(":c", now);
        query.bindValue(":m", now);
        QVERIFY(query.exec());
        int id = query.lastInsertId().toInt();

        // 软删除
        query.prepare("UPDATE notes_todos SET is_deleted=1, deletion_datetime=:d WHERE id=:id");
        query.bindValue(":d", QDateTime::currentSecsSinceEpoch());
        query.bindValue(":id", id);
        QVERIFY(query.exec());

        query.prepare("SELECT is_deleted FROM notes_todos WHERE id=:id");
        query.bindValue(":id", id);
        QVERIFY(query.exec());
        QVERIFY(query.next());
        QCOMPARE(query.value("is_deleted").toInt(), 1);

        // 恢复
        query.prepare("UPDATE notes_todos SET is_deleted=0, deletion_datetime=0 WHERE id=:id");
        query.bindValue(":id", id);
        QVERIFY(query.exec());

        query.prepare("SELECT is_deleted FROM notes_todos WHERE id=:id");
        query.bindValue(":id", id);
        QVERIFY(query.exec());
        QVERIFY(query.next());
        QCOMPARE(query.value("is_deleted").toInt(), 0);
    }

    // 待办 CRUD 测试
    void testCreateTodo()
    {
        QSqlQuery query(m_db);
        qint64 now = QDateTime::currentSecsSinceEpoch();
        qint64 due = now + 86400;
        query.prepare("INSERT INTO notes_todos (title, content, is_todo, priority, is_completed, due_datetime, "
                       "creation_datetime, modification_datetime) "
                       "VALUES (:t, :c, 1, :p, 0, :due, :cr, :mo)");
        query.bindValue(":t", QString("测试待办"));
        query.bindValue(":c", QString(""));
        query.bindValue(":p", 2);
        query.bindValue(":due", due);
        query.bindValue(":cr", now);
        query.bindValue(":mo", now);
        QVERIFY(query.exec());
        int id = query.lastInsertId().toInt();
        QVERIFY(id > 0);

        query.prepare("SELECT * FROM notes_todos WHERE id=:id");
        query.bindValue(":id", id);
        QVERIFY(query.exec());
        QVERIFY(query.next());
        QCOMPARE(query.value("title").toString(), QString("测试待办"));
        QCOMPARE(query.value("is_todo").toInt(), 1);
        QCOMPARE(query.value("priority").toInt(), 2);
        QCOMPARE(query.value("is_completed").toInt(), 0);
    }

    void testToggleComplete()
    {
        QSqlQuery query(m_db);
        qint64 now = QDateTime::currentSecsSinceEpoch();
        query.prepare("INSERT INTO notes_todos (title, is_todo, creation_datetime, modification_datetime) "
                       "VALUES (:t, 1, :c, :m)");
        query.bindValue(":t", QString("完成测试"));
        query.bindValue(":c", now);
        query.bindValue(":m", now);
        QVERIFY(query.exec());
        int id = query.lastInsertId().toInt();

        query.prepare("UPDATE notes_todos SET is_completed=1, completed_datetime=:cd, modification_datetime=:md WHERE id=:id");
        query.bindValue(":cd", QDateTime::currentSecsSinceEpoch());
        query.bindValue(":md", QDateTime::currentSecsSinceEpoch());
        query.bindValue(":id", id);
        QVERIFY(query.exec());

        query.prepare("SELECT is_completed FROM notes_todos WHERE id=:id");
        query.bindValue(":id", id);
        QVERIFY(query.exec());
        QVERIFY(query.next());
        QCOMPARE(query.value("is_completed").toInt(), 1);
    }

    // 标签 CRUD 测试
    void testCreateTag()
    {
        QSqlQuery query(m_db);
        qint64 now = QDateTime::currentSecsSinceEpoch();
        query.prepare("INSERT INTO tags (name, color, created_at) VALUES (:n, :c, :t)");
        query.bindValue(":n", QString("工作"));
        query.bindValue(":c", QString("#1890FF"));
        query.bindValue(":t", now);
        QVERIFY(query.exec());
        QVERIFY(query.lastInsertId().toInt() > 0);
    }

    void testSearchNotes()
    {
        QSqlQuery query(m_db);
        qint64 now = QDateTime::currentSecsSinceEpoch();
        query.prepare("INSERT INTO notes_todos (title, content, is_todo, creation_datetime, modification_datetime) "
                       "VALUES (:t, :c, 0, :cr, :mo)");
        query.bindValue(":t", QString("AI技术方案"));
        query.bindValue(":c", QString("关于深度学习模型的优化方案"));
        query.bindValue(":cr", now);
        query.bindValue(":mo", now);
        QVERIFY(query.exec());

        query.prepare("SELECT * FROM notes_todos WHERE is_deleted=0 AND is_todo=0 AND "
                       "(title LIKE :kw OR content LIKE :kw2)");
        query.bindValue(":kw", QString("%AI%"));
        query.bindValue(":kw2", QString("%AI%"));
        QVERIFY(query.exec());
        QVERIFY(query.next());
        QCOMPARE(query.value("title").toString(), QString("AI技术方案"));
    }

    void testOverdueTodo()
    {
        QSqlQuery query(m_db);
        qint64 now = QDateTime::currentSecsSinceEpoch();
        qint64 pastDue = now - 3600;
        query.prepare("INSERT INTO notes_todos (title, is_todo, due_datetime, is_completed, is_deleted, "
                       "creation_datetime, modification_datetime) "
                       "VALUES (:t, 1, :d, 0, 0, :c, :m)");
        query.bindValue(":t", QString("逾期待办"));
        query.bindValue(":d", pastDue);
        query.bindValue(":c", now);
        query.bindValue(":m", now);
        QVERIFY(query.exec());

        query.prepare("SELECT * FROM notes_todos WHERE is_todo=1 AND is_deleted=0 AND is_completed=0 "
                       "AND due_datetime>0 AND due_datetime<:now ORDER BY due_datetime ASC");
        query.bindValue(":now", now);
        QVERIFY(query.exec());
        QVERIFY(query.next());
        QCOMPARE(query.value("title").toString(), QString("逾期待办"));
    }

    void testConvertNoteToTodo()
    {
        QSqlQuery query(m_db);
        qint64 now = QDateTime::currentSecsSinceEpoch();
        query.prepare("INSERT INTO notes_todos (title, content, is_todo, creation_datetime, modification_datetime) "
                       "VALUES (:t, :c, 0, :cr, :mo)");
        query.bindValue(":t", QString("笔记转待办"));
        query.bindValue(":c", QString(""));
        query.bindValue(":cr", now);
        query.bindValue(":mo", now);
        QVERIFY(query.exec());
        int id = query.lastInsertId().toInt();

        query.prepare("UPDATE notes_todos SET is_todo=1, priority=:p, due_datetime=:due, modification_datetime=:m WHERE id=:id");
        query.bindValue(":p", 2);
        query.bindValue(":due", now + 86400);
        query.bindValue(":m", QDateTime::currentSecsSinceEpoch());
        query.bindValue(":id", id);
        QVERIFY(query.exec());

        query.prepare("SELECT is_todo, priority FROM notes_todos WHERE id=:id");
        query.bindValue(":id", id);
        QVERIFY(query.exec());
        QVERIFY(query.next());
        QCOMPARE(query.value("is_todo").toInt(), 1);
        QCOMPARE(query.value("priority").toInt(), 2);
    }

    void testNoteCount()
    {
        QSqlQuery query(m_db);
        qint64 now = QDateTime::currentSecsSinceEpoch();
        for (int i = 0; i < 3; i++) {
            query.prepare("INSERT INTO notes_todos (title, is_todo, is_deleted, creation_datetime, modification_datetime) "
                           "VALUES (:t, 0, 0, :c, :m)");
            query.bindValue(":t", QString("笔记%1").arg(i));
            query.bindValue(":c", now + i);
            query.bindValue(":m", now + i);
            QVERIFY(query.exec());
        }

        QVERIFY(query.exec("SELECT COUNT(*) FROM notes_todos WHERE is_deleted=0 AND is_todo=0"));
        QVERIFY(query.next());
        QVERIFY(query.value(0).toInt() >= 3);
    }

    void testTodoCount()
    {
        QSqlQuery query(m_db);
        qint64 now = QDateTime::currentSecsSinceEpoch();
        query.prepare("INSERT INTO notes_todos (title, is_todo, is_deleted, is_completed, creation_datetime, modification_datetime) "
                       "VALUES (:t, 1, 0, 0, :c, :m)");
        query.bindValue(":t", QString("待办计数"));
        query.bindValue(":c", now);
        query.bindValue(":m", now);
        QVERIFY(query.exec());

        QVERIFY(query.exec("SELECT COUNT(*) FROM notes_todos WHERE is_todo=1 AND is_completed=0 AND is_deleted=0"));
        QVERIFY(query.next());
        QVERIFY(query.value(0).toInt() >= 1);
    }
};

QTEST_MAIN(TestStorage)
#include "test_storage.moc"