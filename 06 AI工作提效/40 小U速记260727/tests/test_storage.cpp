#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include "storage/database.h"
#include "storage/todostorage.h"

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
    if (!query.exec(tagsSql)) return false;

    const QString todoTagsSql = R"(
        CREATE TABLE IF NOT EXISTS todo_tags (
            todo_id INTEGER NOT NULL,
            tag_id  INTEGER NOT NULL,
            PRIMARY KEY (todo_id, tag_id),
            FOREIGN KEY (todo_id) REFERENCES notes_todos(id),
            FOREIGN KEY (tag_id) REFERENCES tags(id)
        )
    )";
    return query.exec(todoTagsSql);
}

// 测试用 Database 子类：注入内存连接，使 TodoStorage 可被直接单测
class TestDatabase : public Database
{
public:
    explicit TestDatabase(QSqlDatabase &conn)
        : Database(nullptr), m_conn(conn) {}
    QSqlDatabase &connection() override { return m_conn; }

private:
    QSqlDatabase &m_conn;
};

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

    // 回收站搜索：只匹配已删除笔记，且不包含未删除笔记
    void testSearchDeletedNotes()
    {
        QSqlQuery query(m_db);
        qint64 now = QDateTime::currentSecsSinceEpoch();

        // 已删除笔记（标题含关键词）
        query.prepare("INSERT INTO notes_todos (title, content, is_todo, is_deleted, deletion_datetime, "
                       "creation_datetime, modification_datetime) "
                       "VALUES (:t, :c, 0, 1, :d, :cr, :mo)");
        query.bindValue(":t", QString("待恢复的旧方案"));
        query.bindValue(":c", QString("废弃的深度学习草案"));
        query.bindValue(":d", now);
        query.bindValue(":cr", now);
        query.bindValue(":mo", now);
        QVERIFY(query.exec());

        // 已删除笔记（内容含关键词，标题不含）
        query.prepare("INSERT INTO notes_todos (title, content, is_todo, is_deleted, deletion_datetime, "
                       "creation_datetime, modification_datetime) "
                       "VALUES (:t, :c, 0, 1, :d, :cr, :mo)");
        query.bindValue(":t", QString("草稿"));
        query.bindValue(":c", QString("这里包含关键词 旧方案 的正文"));
        query.bindValue(":d", now);
        query.bindValue(":cr", now);
        query.bindValue(":mo", now);
        QVERIFY(query.exec());

        // 未删除笔记（标题含关键词，不应被回收站搜索命中）
        query.prepare("INSERT INTO notes_todos (title, content, is_todo, is_deleted, "
                       "creation_datetime, modification_datetime) "
                       "VALUES (:t, :c, 0, 0, :cr, :mo)");
        query.bindValue(":t", QString("旧方案-正常笔记"));
        query.bindValue(":c", QString(""));
        query.bindValue(":cr", now);
        query.bindValue(":mo", now);
        QVERIFY(query.exec());

        // 回收站内按关键词搜索（与 NoteStorage::searchDeletedNotes 相同的 SQL）
        query.prepare("SELECT * FROM notes_todos WHERE is_deleted = 1 AND (title LIKE :kw OR content LIKE :kw2) "
                       "ORDER BY modification_datetime DESC");
        query.bindValue(":kw", QString("%旧方案%"));
        query.bindValue(":kw2", QString("%旧方案%"));
        QVERIFY(query.exec());

        QStringList titles;
        while (query.next()) {
            titles << query.value("title").toString();
        }
        // 应命中 2 条已删除笔记，且不包含未删除的「旧方案-正常笔记」
        QCOMPARE(titles.size(), 2);
        QVERIFY(titles.contains(QString("待恢复的旧方案")));
        QVERIFY(titles.contains(QString("草稿")));
        QVERIFY(!titles.contains(QString("旧方案-正常笔记")));
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

    // 周报标签统计（IDE-191）：多标签/单标签/无标签/范围外
    void testTagStats()
    {
        TestDatabase testDb(m_db);
        TodoStorage storage(&testDb);

        const qint64 dayStart = QDateTime(QDate(2026, 7, 1), QTime(0, 0)).toSecsSinceEpoch();
        const qint64 dayEnd = QDateTime(QDate(2026, 7, 7), QTime(23, 59, 59)).toSecsSinceEpoch();

        // 单标签、未完成
        TodoData a;
        a.title = "写周报";
        a.dueDatetime = QDateTime(QDate(2026, 7, 2), QTime(12, 0)).toSecsSinceEpoch();
        a.creationDatetime = a.dueDatetime - 86400;
        a.modificationDatetime = a.creationDatetime;
        a.tags = {"工作"};
        QVERIFY(storage.createTodo(a) > 0);

        // 多标签（生活+工作）、已完成
        TodoData b;
        b.title = "买菜";
        b.dueDatetime = QDateTime(QDate(2026, 7, 3), QTime(12, 0)).toSecsSinceEpoch();
        b.creationDatetime = b.dueDatetime - 86400;
        b.modificationDatetime = b.creationDatetime;
        b.tags = {"生活", "工作"};
        b.isCompleted = true;
        QVERIFY(storage.createTodo(b) > 0);

        // 范围外（7月20日）不计入
        TodoData c;
        c.title = "远期任务";
        c.dueDatetime = QDateTime(QDate(2026, 7, 20), QTime(12, 0)).toSecsSinceEpoch();
        c.creationDatetime = c.dueDatetime - 86400;
        c.modificationDatetime = c.creationDatetime;
        c.tags = {"工作"};
        QVERIFY(storage.createTodo(c) > 0);

        // 无标签 → 归入"未分类"
        TodoData d;
        d.title = "无标签事项";
        d.dueDatetime = QDateTime(QDate(2026, 7, 4), QTime(12, 0)).toSecsSinceEpoch();
        d.creationDatetime = d.dueDatetime - 86400;
        d.modificationDatetime = d.creationDatetime;
        QVERIFY(storage.createTodo(d) > 0);

        QList<TagStat> stats = storage.getTagStats(dayStart, dayEnd);
        QCOMPARE(stats.size(), 3);

        QMap<QString, TagStat> byTag;
        for (const auto &st : stats) {
            byTag[st.tag] = st;
        }

        QVERIFY(byTag.contains("工作"));
        QCOMPARE(byTag["工作"].total, 2);
        QCOMPARE(byTag["工作"].completed, 1);
        QVERIFY(qFuzzyCompare(byTag["工作"].rate(), 50.0));

        QVERIFY(byTag.contains("生活"));
        QCOMPARE(byTag["生活"].total, 1);
        QCOMPARE(byTag["生活"].completed, 1);
        QVERIFY(qFuzzyCompare(byTag["生活"].rate(), 100.0));

        QVERIFY(byTag.contains("未分类"));
        QCOMPARE(byTag["未分类"].total, 1);
        QCOMPARE(byTag["未分类"].completed, 0);
        QVERIFY(qFuzzyCompare(byTag["未分类"].rate(), 0.0));
    }
};

QTEST_MAIN(TestStorage)
#include "test_storage.moc"