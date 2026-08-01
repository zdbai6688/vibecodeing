#include "migration.h"
#include "storage/database.h"
#include "core/notemanager.h"
#include "core/tagmanager.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

Migration::Migration(Database *db, NoteManager *noteMgr, TagManager *tagMgr, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_noteManager(noteMgr)
    , m_tagManager(tagMgr)
{
}

QString Migration::voiceNoteDbPath()
{
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QStringList candidates = {
        home + "/.local/share/deepin-voice-note/data/vnotedb.db",
        home + "/.local/share/deepin-voice-note/vnotedb.db",
        home + "/.local/share/deepin-voice-note/data/notes.db",
    };
    for (const QString &path : candidates) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    return QString();
}

bool Migration::hasVoiceNoteData() const
{
    return !voiceNoteDbPath().isEmpty();
}

QList<Migration::VNoteRecord> Migration::readVoiceNoteRecords() const
{
    QList<VNoteRecord> records;
    QString dbPath = voiceNoteDbPath();
    if (dbPath.isEmpty()) return records;

    QSqlDatabase srcDb = QSqlDatabase::addDatabase("QSQLITE", "voice_note_src");
    srcDb.setDatabaseName(dbPath);

    if (!srcDb.open()) {
        qWarning() << "无法打开deepin-voice-note数据库:" << srcDb.lastError().text();
        return records;
    }

    QSqlQuery query(srcDb);
    QStringList tables = srcDb.tables();
    qInfo() << "deepin-voice-note数据库表:" << tables;

    if (tables.contains("vnotedb")) {
        if (query.exec("SELECT * FROM vnotedb")) {
            while (query.next()) {
                VNoteRecord rec;
                rec.title = query.value("title").toString();
                rec.content = query.value("content").toString();
                rec.createdAt = query.value("createtime").toLongLong();
                rec.modifiedAt = query.value("modifytime").toLongLong();
                rec.isDeleted = query.value("isdeleted").toInt() == 1;
                rec.folderName = query.value("folder").toString();
                records.append(rec);
            }
        }
    } else if (tables.contains("notes")) {
        if (query.exec("SELECT * FROM notes")) {
            while (query.next()) {
                VNoteRecord rec;
                rec.title = query.value("title").toString();
                rec.content = query.value("content").toString();
                rec.createdAt = query.value("created_at").toLongLong();
                rec.modifiedAt = query.value("updated_at").toLongLong();
                rec.isDeleted = false;
                rec.folderName = query.value("folder_name").toString();
                records.append(rec);
            }
        }
    }

    srcDb.close();
    QSqlDatabase::removeDatabase("voice_note_src");
    return records;
}

int Migration::importVoiceNoteData()
{
    if (!hasVoiceNoteData()) {
        qInfo() << "未找到deepin-voice-note数据";
        return 0;
    }

    m_importCount = 0;
    emit importStarted();

    QList<VNoteRecord> records = readVoiceNoteRecords();
    int total = records.size();
    qInfo() << "发现" << total << "条deepin-voice-note记录";

    for (int i = 0; i < total; ++i) {
        const auto &rec = records[i];

        NoteData note;
        note.title = rec.title.isEmpty() ? tr("从语音笔记导入") : rec.title;
        note.content = rec.content;
        note.contentType = "markdown";
        note.creationDatetime = rec.createdAt > 0 ? rec.createdAt : QDateTime::currentSecsSinceEpoch();
        note.modificationDatetime = rec.modifiedAt > 0 ? rec.modifiedAt : note.creationDatetime;
        note.isDeleted = rec.isDeleted;

        if (!rec.folderName.isEmpty()) {
            note.tag = rec.folderName;
            QStringList existing = m_tagManager->allTagNames();
            if (!existing.contains(rec.folderName, Qt::CaseInsensitive)) {
                m_tagManager->createTag(rec.folderName, "#1890FF");
            }
        }

        int id = m_noteManager->createNote(note);
        if (id > 0) {
            m_importCount++;
        }

        emit importProgress(i + 1, total);
    }

    qInfo() << "导入完成，共导入" << m_importCount << "条记录";
    emit importFinished(m_importCount);
    return m_importCount;
}