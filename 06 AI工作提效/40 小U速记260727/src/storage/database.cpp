// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "database.h"

#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDebug>

Database::Database(QObject *parent)
    : QObject(parent)
{
    m_dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(m_dataPath);
}

Database::~Database()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool Database::initialize()
{
    QString dbPath = m_dataPath + "/uos-shorthand.db";

    m_db = QSqlDatabase::addDatabase("QSQLITE", "main_connection");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qCritical() << "无法打开数据库:" << m_db.lastError().text();
        return false;
    }

    // 启用WAL模式提升性能
    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA synchronous=NORMAL");
    pragma.exec("PRAGMA foreign_keys=ON");

    // 数据库架构版本管理
    int currentVersion = 3;
    QSqlQuery versionQuery(m_db);
    versionQuery.exec("PRAGMA user_version");
    int dbVersion = 0;
    if (versionQuery.next()) {
        dbVersion = versionQuery.value(0).toInt();
    }

    if (dbVersion < currentVersion) {
        qInfo() << "数据库架构升级:" << dbVersion << "->" << currentVersion;
        if (dbVersion < 2) {
            // v1 → v2: 新增 todo_tags 多对多关联表，迁移旧 tag 数据
            QSqlQuery migrate(m_db);
            migrate.exec(R"(
                CREATE TABLE IF NOT EXISTS todo_tags (
                    todo_id INTEGER NOT NULL,
                    tag_id  INTEGER NOT NULL,
                    PRIMARY KEY (todo_id, tag_id),
                    FOREIGN KEY (todo_id) REFERENCES notes_todos(id),
                    FOREIGN KEY (tag_id) REFERENCES tags(id)
                )
            )");
            if (migrate.lastError().isValid()) {
                qWarning() << "创建 todo_tags 表失败:" << migrate.lastError().text();
            } else {
                qInfo() << "todo_tags 表创建成功";
                // 迁移现有 tag 数据到 todo_tags
                QSqlQuery migrateData(m_db);
                migrateData.exec(R"(
                    INSERT OR IGNORE INTO todo_tags (todo_id, tag_id)
                    SELECT n.id, t.id
                    FROM notes_todos n, tags t
                    WHERE n.is_todo = 1 AND n.tag != '' AND n.tag = t.name
                )");
                qInfo() << "标签数据迁移完成";
            }
        }
        if (dbVersion < 3) {
            // v2 → v3: meetings 表增加回收站字段（is_deleted / deleted_at），支持会议软删除进回收站
            QSqlQuery alter(m_db);
            QList<QPair<QString, QString>> cols = {
                {"is_deleted", "INTEGER DEFAULT 0"},
                {"deleted_at", "INTEGER DEFAULT 0"},
            };
            for (const auto &c : cols) {
                alter.exec(QString("ALTER TABLE meetings ADD COLUMN %1 %2").arg(c.first, c.second));
                if (alter.lastError().isValid()
                    && !alter.lastError().text().contains("duplicate column", Qt::CaseInsensitive)) {
                    qWarning() << "添加 meetings 列失败:" << alter.lastError().text();
                }
            }
        }

        QSqlQuery setVersion(m_db);
        setVersion.exec(QString("PRAGMA user_version=%1").arg(currentVersion));
    }

    if (!createTables()) {
        return false;
    }

    qInfo() << "数据库已打开:" << dbPath;
    return true;
}

QSqlDatabase &Database::connection()
{
    return m_db;
}

bool Database::createTables()
{
    QSqlQuery query(m_db);

    // 笔记/待办表
    const QString createNotes = R"(
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

    if (!query.exec(createNotes)) {
        qCritical() << "创建notes_todos表失败:" << query.lastError().text();
        return false;
    }

    // 文件夹表
    const QString createFolders = R"(
        CREATE TABLE IF NOT EXISTS folders (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT NOT NULL,
            parent_id   INTEGER DEFAULT 0,
            sort_order  INTEGER DEFAULT 0,
            created_at  INTEGER NOT NULL
        )
    )";

    if (!query.exec(createFolders)) {
        qCritical() << "创建folders表失败:" << query.lastError().text();
        return false;
    }

    // 标签表
    const QString createTags = R"(
        CREATE TABLE IF NOT EXISTS tags (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT NOT NULL UNIQUE,
            color       TEXT DEFAULT '#1890FF',
            created_at  INTEGER NOT NULL
        )
    )";

    if (!query.exec(createTags)) {
        qCritical() << "创建tags表失败:" << query.lastError().text();
        return false;
    }

    // 待办-标签多对多关联表
    const QString createTodoTags = R"(
        CREATE TABLE IF NOT EXISTS todo_tags (
            todo_id INTEGER NOT NULL,
            tag_id  INTEGER NOT NULL,
            PRIMARY KEY (todo_id, tag_id),
            FOREIGN KEY (todo_id) REFERENCES notes_todos(id),
            FOREIGN KEY (tag_id) REFERENCES tags(id)
        )
    )";

    if (!query.exec(createTodoTags)) {
        qCritical() << "创建todo_tags表失败:" << query.lastError().text();
        return false;
    }

    // 设置表
    const QString createSettings = R"(
        CREATE TABLE IF NOT EXISTS settings (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        )
    )";

    if (!query.exec(createSettings)) {
        qCritical() << "创建settings表失败:" << query.lastError().text();
        return false;
    }

    // 会议记录表
    const QString createMeetings = R"(
        CREATE TABLE IF NOT EXISTS meetings (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            title           TEXT NOT NULL DEFAULT '',
            started_at      INTEGER NOT NULL,
            ended_at        INTEGER DEFAULT 0,
            duration_secs   INTEGER DEFAULT 0,
            audio_file_path TEXT DEFAULT '',
            ai_summary      TEXT DEFAULT '',
            manual_notes    TEXT DEFAULT '',
            status          TEXT DEFAULT 'completed',
            created_at      INTEGER NOT NULL,
            is_deleted      INTEGER DEFAULT 0,
            deleted_at      INTEGER DEFAULT 0
        )
    )";

    if (!query.exec(createMeetings)) {
        qCritical() << "创建meetings表失败:" << query.lastError().text();
        return false;
    }

    const QString createTranscripts = R"(
        CREATE TABLE IF NOT EXISTS meeting_transcripts (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            meeting_id  INTEGER NOT NULL,
            sequence    INTEGER NOT NULL,
            speaker     TEXT DEFAULT '',
            text        TEXT NOT NULL,
            timestamp_ms INTEGER NOT NULL,
            created_at  INTEGER NOT NULL,
            FOREIGN KEY (meeting_id) REFERENCES meetings(id)
        )
    )";

    if (!query.exec(createTranscripts)) {
        qCritical() << "创建meeting_transcripts表失败:" << query.lastError().text();
        return false;
    }

    const QString createStickyNotes = R"(
        CREATE TABLE IF NOT EXISTS sticky_notes (
            note_id       INTEGER PRIMARY KEY,
            sticky_x      INTEGER DEFAULT 0,
            sticky_y      INTEGER DEFAULT 0,
            sticky_w      INTEGER DEFAULT 280,
            sticky_h      INTEGER DEFAULT 160,
            sticky_color  TEXT DEFAULT '#409EFF',
            is_visible    INTEGER DEFAULT 1,
            FOREIGN KEY (note_id) REFERENCES notes_todos(id)
        )
    )";

    if (!query.exec(createStickyNotes)) {
        qCritical() << "创建sticky_notes表失败:" << query.lastError().text();
        return false;
    }

    // 索引
    query.exec("CREATE INDEX IF NOT EXISTS idx_notes_deleted ON notes_todos(is_deleted)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_notes_todo ON notes_todos(is_todo)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_notes_folder ON notes_todos(folder_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_notes_modtime ON notes_todos(modification_datetime)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_todo_tags_todo ON todo_tags(todo_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_todo_tags_tag ON todo_tags(tag_id)");

    // 插入默认标签
    query.exec("INSERT OR IGNORE INTO tags (name, color, created_at) VALUES ('个人', '#1890FF', 0)");
    query.exec("INSERT OR IGNORE INTO tags (name, color, created_at) VALUES ('工作', '#52C41A', 0)");
    query.exec("INSERT OR IGNORE INTO tags (name, color, created_at) VALUES ('日常', '#FAAD14', 0)");

    return true;
}
