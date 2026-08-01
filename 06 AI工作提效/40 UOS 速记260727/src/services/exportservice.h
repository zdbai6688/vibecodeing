#ifndef EXPORTSERVICE_H
#define EXPORTSERVICE_H

#include <QObject>
#include <QString>
#include "storage/notestorage.h"
#include "storage/meetingstorage.h"

class ExportService : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ExportService)

public:
    explicit ExportService(QObject *parent = nullptr);

    bool exportNoteToMarkdown(const NoteData &note, const QString &filePath, bool includeMeta = true);
    bool exportNoteToTxt(const NoteData &note, const QString &filePath);
    bool exportNotesToZip(const QList<NoteData> &notes, const QString &zipPath);
    bool exportMeeting(const MeetingData &meeting, const QString &dirPath);
    static QString sanitizeFileName(const QString &name);
};

#endif // EXPORTSERVICE_H