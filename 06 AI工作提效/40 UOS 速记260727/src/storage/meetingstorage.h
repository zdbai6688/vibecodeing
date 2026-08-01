#ifndef MEETINGSTORAGE_H
#define MEETINGSTORAGE_H

#include <QObject>
#include <QList>
#include <QDateTime>

class Database;

struct MeetingData {
    int id = 0;
    QString title;
    qint64 startedAt = 0;
    qint64 endedAt = 0;
    int durationSecs = 0;
    QString audioFilePath;
    QString aiSummary;
    QString manualNotes;
    QString status = "completed";
    qint64 createdAt = 0;

    QDateTime startedTime() const { return QDateTime::fromSecsSinceEpoch(startedAt); }
    QDateTime createdTime() const { return QDateTime::fromSecsSinceEpoch(createdAt); }
    QString formattedDuration() const;
};

struct TranscriptData {
    int id = 0;
    int meetingId = 0;
    int sequence = 0;
    QString speaker;
    QString text;
    qint64 timestampMs = 0;
    qint64 createdAt = 0;

    QString formattedTimestamp() const;
};

class MeetingStorage : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(MeetingStorage)

public:
    explicit MeetingStorage(Database *db, QObject *parent = nullptr);

    int createMeeting(const MeetingData &meeting);
    bool updateMeeting(const MeetingData &meeting);
    bool deleteMeeting(int id);

    MeetingData getMeeting(int id) const;
    QList<MeetingData> getAllMeetings() const;
    QList<MeetingData> searchMeetings(const QString &keyword) const;

    int addTranscript(const TranscriptData &transcript);
    QList<TranscriptData> getTranscripts(int meetingId) const;
    bool deleteTranscripts(int meetingId);

private:
    MeetingData rowToMeeting(const QVariantMap &row) const;
    TranscriptData rowToTranscript(const QVariantMap &row) const;
    Database *m_db;
};

#endif // MEETINGSTORAGE_H