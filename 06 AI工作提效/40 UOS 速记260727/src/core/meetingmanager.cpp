#include "meetingmanager.h"
#include "storage/database.h"

MeetingManager::MeetingManager(Database *db, QObject *parent)
    : QObject(parent)
{
    m_storage = new MeetingStorage(db, this);
}

int MeetingManager::createMeeting(const MeetingData &meeting)
{
    int id = m_storage->createMeeting(meeting);
    if (id > 0) {
        emit meetingCreated(id);
        emit dataChanged();
    }
    return id;
}

bool MeetingManager::updateMeeting(const MeetingData &meeting)
{
    bool ok = m_storage->updateMeeting(meeting);
    if (ok) {
        emit meetingUpdated(meeting.id);
        emit dataChanged();
    }
    return ok;
}

bool MeetingManager::deleteMeeting(int id)
{
    bool ok = m_storage->deleteMeeting(id);
    if (ok) {
        emit meetingDeleted(id);
        emit dataChanged();
    }
    return ok;
}

MeetingData MeetingManager::getMeeting(int id) const { return m_storage->getMeeting(id); }
QList<MeetingData> MeetingManager::getAllMeetings() const { return m_storage->getAllMeetings(); }
QList<MeetingData> MeetingManager::searchMeetings(const QString &keyword) const { return m_storage->searchMeetings(keyword); }

int MeetingManager::addTranscript(const TranscriptData &transcript) { return m_storage->addTranscript(transcript); }
QList<TranscriptData> MeetingManager::getTranscripts(int meetingId) const { return m_storage->getTranscripts(meetingId); }
bool MeetingManager::deleteTranscripts(int meetingId) { return m_storage->deleteTranscripts(meetingId); }

int MeetingManager::meetingCount() const { return m_storage->getAllMeetings().size(); }