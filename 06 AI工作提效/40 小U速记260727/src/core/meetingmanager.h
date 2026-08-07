#ifndef MEETINGMANAGER_H
#define MEETINGMANAGER_H

#include <QObject>
#include "storage/meetingstorage.h"

class Database;

class MeetingManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(MeetingManager)

public:
    explicit MeetingManager(Database *db, QObject *parent = nullptr);

    int createMeeting(const MeetingData &meeting);
    bool updateMeeting(const MeetingData &meeting);
    bool deleteMeeting(int id);
    /// 批量删除会议记录
    bool batchDeleteMeetings(const QList<int> &ids);
    /// 恢复回收站中的会议
    bool restoreMeeting(int id);
    /// 永久删除会议
    bool permanentDeleteMeeting(int id);
    /// 清空回收站中的会议
    bool permanentDeleteAllMeetings();

    MeetingData getMeeting(int id) const;
    QList<MeetingData> getAllMeetings() const;
    QList<MeetingData> getDeletedMeetings() const;
    QList<MeetingData> searchMeetings(const QString &keyword) const;

    int addTranscript(const TranscriptData &transcript);
    QList<TranscriptData> getTranscripts(int meetingId) const;
    bool deleteTranscripts(int meetingId);

    int meetingCount() const;

signals:
    void meetingCreated(int id);
    void meetingUpdated(int id);
    void meetingDeleted(int id);
    void dataChanged();

private:
    MeetingStorage *m_storage;
};

#endif // MEETINGMANAGER_H