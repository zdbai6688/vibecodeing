#include "meetingstorage.h"
#include "database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>

QString MeetingData::formattedDuration() const
{
    int h = durationSecs / 3600;
    int m = (durationSecs % 3600) / 60;
    int s = durationSecs % 60;
    if (h > 0) return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
}

QString TranscriptData::formattedTimestamp() const
{
    int totalSecs = timestampMs / 1000;
    int m = totalSecs / 60;
    int s = totalSecs % 60;
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

static QVariantMap queryToMap(const QSqlQuery &query)
{
    QVariantMap map;
    for (int i = 0; i < query.record().count(); ++i) {
        map[query.record().fieldName(i)] = query.value(i);
    }
    return map;
}

MeetingStorage::MeetingStorage(Database *db, QObject *parent)
    : QObject(parent), m_db(db)
{
}

int MeetingStorage::createMeeting(const MeetingData &meeting)
{
    QSqlQuery query(m_db->connection());
    query.prepare(R"(
        INSERT INTO meetings (title, started_at, ended_at, duration_secs, audio_file_path,
                              ai_summary, manual_notes, status, created_at)
        VALUES (:title, :started, :ended, :duration, :audio, :summary, :notes, :status, :created)
    )");
    qint64 now = QDateTime::currentSecsSinceEpoch();
    query.bindValue(":title", meeting.title);
    query.bindValue(":started", meeting.startedAt > 0 ? meeting.startedAt : now);
    query.bindValue(":ended", meeting.endedAt);
    query.bindValue(":duration", meeting.durationSecs);
    query.bindValue(":audio", meeting.audioFilePath);
    query.bindValue(":summary", meeting.aiSummary);
    query.bindValue(":notes", meeting.manualNotes);
    query.bindValue(":status", meeting.status);
    query.bindValue(":created", now);

    if (!query.exec()) {
        qWarning() << "创建会议失败:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

bool MeetingStorage::updateMeeting(const MeetingData &meeting)
{
    QSqlQuery query(m_db->connection());
    query.prepare(R"(
        UPDATE meetings SET title=:title, started_at=:started, ended_at=:ended,
               duration_secs=:duration, audio_file_path=:audio, ai_summary=:summary,
               manual_notes=:notes, status=:status
        WHERE id=:id
    )");
    query.bindValue(":title", meeting.title);
    query.bindValue(":started", meeting.startedAt);
    query.bindValue(":ended", meeting.endedAt);
    query.bindValue(":duration", meeting.durationSecs);
    query.bindValue(":audio", meeting.audioFilePath);
    query.bindValue(":summary", meeting.aiSummary);
    query.bindValue(":notes", meeting.manualNotes);
    query.bindValue(":status", meeting.status);
    query.bindValue(":id", meeting.id);

    if (!query.exec()) {
        qWarning() << "更新会议失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool MeetingStorage::deleteMeeting(int id)
{
    QSqlQuery query(m_db->connection());
    query.prepare("DELETE FROM meeting_transcripts WHERE meeting_id=:mid");
    query.bindValue(":mid", id);
    query.exec();

    query.prepare("DELETE FROM meetings WHERE id=:id");
    query.bindValue(":id", id);
    return query.exec();
}

bool MeetingStorage::batchDeleteMeetings(const QList<int> &ids)
{
    if (ids.isEmpty()) return true;
    
    QSqlQuery query(m_db->connection());
    // 构建占位符列表
    QStringList placeholders;
    for (int i = 0; i < ids.size(); ++i) {
        placeholders << QString(":id%1").arg(i);
    }
    QString placeholdersStr = placeholders.join(", ");
    
    // 删除关联转写
    query.prepare(QString("DELETE FROM meeting_transcripts WHERE meeting_id IN (%1)").arg(placeholdersStr));
    for (int i = 0; i < ids.size(); ++i) {
        query.bindValue(QString(":id%1").arg(i), ids[i]);
    }
    query.exec();
    
    // 删除会议记录
    query.prepare(QString("DELETE FROM meetings WHERE id IN (%1)").arg(placeholdersStr));
    for (int i = 0; i < ids.size(); ++i) {
        query.bindValue(QString(":id%1").arg(i), ids[i]);
    }
    return query.exec();
}

MeetingData MeetingStorage::getMeeting(int id) const
{
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM meetings WHERE id=:id");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        return rowToMeeting(queryToMap(query));
    }
    return MeetingData();
}

QList<MeetingData> MeetingStorage::getAllMeetings() const
{
    QList<MeetingData> list;
    QSqlQuery query(m_db->connection());
    if (query.exec("SELECT * FROM meetings ORDER BY created_at DESC")) {
        while (query.next()) {
            list.append(rowToMeeting(queryToMap(query)));
        }
    }
    return list;
}

QList<MeetingData> MeetingStorage::searchMeetings(const QString &keyword) const
{
    QList<MeetingData> list;
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM meetings WHERE title LIKE :kw OR ai_summary LIKE :kw2 ORDER BY created_at DESC");
    QString like = "%" + keyword + "%";
    query.bindValue(":kw", like);
    query.bindValue(":kw2", like);
    if (query.exec()) {
        while (query.next()) {
            list.append(rowToMeeting(queryToMap(query)));
        }
    }
    return list;
}

int MeetingStorage::addTranscript(const TranscriptData &transcript)
{
    QSqlQuery query(m_db->connection());
    query.prepare(R"(
        INSERT INTO meeting_transcripts (meeting_id, sequence, speaker, text, timestamp_ms, created_at)
        VALUES (:mid, :seq, :speaker, :text, :ts, :created)
    )");
    query.bindValue(":mid", transcript.meetingId);
    query.bindValue(":seq", transcript.sequence);
    query.bindValue(":speaker", transcript.speaker);
    query.bindValue(":text", transcript.text);
    query.bindValue(":ts", transcript.timestampMs);
    query.bindValue(":created", QDateTime::currentSecsSinceEpoch());

    if (!query.exec()) {
        qWarning() << "添加转写失败:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

QList<TranscriptData> MeetingStorage::getTranscripts(int meetingId) const
{
    QList<TranscriptData> list;
    QSqlQuery query(m_db->connection());
    query.prepare("SELECT * FROM meeting_transcripts WHERE meeting_id=:mid ORDER BY sequence ASC");
    query.bindValue(":mid", meetingId);
    if (query.exec()) {
        while (query.next()) {
            list.append(rowToTranscript(queryToMap(query)));
        }
    }
    return list;
}

bool MeetingStorage::deleteTranscripts(int meetingId)
{
    QSqlQuery query(m_db->connection());
    query.prepare("DELETE FROM meeting_transcripts WHERE meeting_id=:mid");
    query.bindValue(":mid", meetingId);
    return query.exec();
}

MeetingData MeetingStorage::rowToMeeting(const QVariantMap &row) const
{
    MeetingData m;
    m.id = row.value("id").toInt();
    m.title = row.value("title").toString();
    m.startedAt = row.value("started_at").toLongLong();
    m.endedAt = row.value("ended_at").toLongLong();
    m.durationSecs = row.value("duration_secs").toInt();
    m.audioFilePath = row.value("audio_file_path").toString();
    m.aiSummary = row.value("ai_summary").toString();
    m.manualNotes = row.value("manual_notes").toString();
    m.status = row.value("status").toString();
    m.createdAt = row.value("created_at").toLongLong();
    return m;
}

TranscriptData MeetingStorage::rowToTranscript(const QVariantMap &row) const
{
    TranscriptData t;
    t.id = row.value("id").toInt();
    t.meetingId = row.value("meeting_id").toInt();
    t.sequence = row.value("sequence").toInt();
    t.speaker = row.value("speaker").toString();
    t.text = row.value("text").toString();
    t.timestampMs = row.value("timestamp_ms").toLongLong();
    t.createdAt = row.value("created_at").toLongLong();
    return t;
}