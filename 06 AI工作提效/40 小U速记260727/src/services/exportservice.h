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
    bool exportNoteToPdf(const NoteData &note, const QString &filePath);
    bool exportNotesToZip(const QList<NoteData> &notes, const QString &zipPath);
    bool exportMeeting(const MeetingData &meeting, const QString &dirPath);
    /// 导出会议录音文件到目标路径(统一文件写入逻辑)
    bool exportMeetingAudio(const QString &sourceFilePath, const QString &destPath);
    /// 导出周报内容(Markdown/纯文本)到文件
    bool exportWeeklyReport(const QString &content, const QString &filePath);
    static QString sanitizeFileName(const QString &name);
};

#endif // EXPORTSERVICE_H
