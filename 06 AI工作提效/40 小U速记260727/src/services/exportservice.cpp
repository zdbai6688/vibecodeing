#include "exportservice.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPdfWriter>
#include <QPainter>

ExportService::ExportService(QObject *parent)
    : QObject(parent)
{
}

QString ExportService::sanitizeFileName(const QString &name)
{
    QString safe = name;
    safe.replace(QRegularExpression("[/\\\\:*?\"<>|]"), "_");
    if (safe.length() > 200) {
        safe = safe.left(200);
    }
    return safe;
}

bool ExportService::exportNoteToMarkdown(const NoteData &note, const QString &filePath, bool includeMeta)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法写入文件:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    if (includeMeta) {
        out << "---\n";
        out << "title: " << note.title << "\n";
        out << "created: " << note.createdAt().toString("yyyy-MM-dd hh:mm:ss") << "\n";
        out << "updated: " << note.modifiedAt().toString("yyyy-MM-dd hh:mm:ss") << "\n";
        if (!note.tag.isEmpty()) {
            out << "tags: " << note.tag << "\n";
        }
        out << "---\n\n";
    }

    out << "# " << note.title << "\n\n";
    out << note.content;

    if (!note.content.endsWith("\n")) {
        out << "\n";
    }

    file.close();
    return true;
}

bool ExportService::exportNoteToTxt(const NoteData &note, const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法写入文件:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << note.title << "\n";
    out << QString("=").repeated(note.title.length()) << "\n\n";
    out << note.content << "\n";
    out << "\n---\n";
    out << "创建时间: " << note.createdAt().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    if (!note.tag.isEmpty()) {
        out << "标签: " << note.tag << "\n";
    }
    file.close();
    return true;
}

bool ExportService::exportNotesToZip(const QList<NoteData> &notes, const QString &zipPath)
{
    if (notes.isEmpty()) return false;

    QString tmpDir = QDir::tempPath() + "/uos-shorthand-export-" + QString::number(QDateTime::currentSecsSinceEpoch());
    QDir().mkpath(tmpDir);

    for (const auto &note : notes) {
        QString safeName = sanitizeFileName(note.title);
        if (safeName.isEmpty()) safeName = "未命名笔记";
        QString filePath = tmpDir + "/" + safeName + ".md";
        exportNoteToMarkdown(note, filePath, true);
    }

    QProcess zip;
    zip.start("zip", {"-j", "-q", zipPath, tmpDir + "/*"});
    zip.waitForFinished(30000);

    QDir(tmpDir).removeRecursively();

    return zip.exitCode() == 0;
}

bool ExportService::exportMeeting(const MeetingData &meeting, const QString &dirPath)
{
    QDir().mkpath(dirPath);

    QString safeName = sanitizeFileName(meeting.title);
    if (safeName.isEmpty()) safeName = "会议记录";

    QFile file(dirPath + "/" + safeName + ".md");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "# " << meeting.title << "\n\n";
    out << "**时间**: " << meeting.createdTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << "**时长**: " << meeting.formattedDuration() << "\n";
    out << "**状态**: " << meeting.status << "\n\n";

    if (!meeting.aiSummary.isEmpty()) {
        out << "## AI 纪要\n\n" << meeting.aiSummary << "\n\n";
    }

    if (!meeting.manualNotes.isEmpty()) {
        out << "## 手动笔记\n\n" << meeting.manualNotes << "\n\n";
    }

    if (!meeting.audioFilePath.isEmpty()) {
        QString audioDest = dirPath + "/" + safeName + ".wav";
        QFile::copy(meeting.audioFilePath, audioDest);
    }

    file.close();
    return true;
}

bool ExportService::exportMeetingAudio(const QString &sourceFilePath, const QString &destPath)
{
    if (sourceFilePath.isEmpty() || !QFile::exists(sourceFilePath)) {
        qWarning() << "录音文件不存在:" << sourceFilePath;
        return false;
    }
    if (destPath.isEmpty()) {
        qWarning() << "导出目标路径为空";
        return false;
    }

    QDir().mkpath(QFileInfo(destPath).absolutePath());
    if (QFile::exists(destPath)) {
        QFile::remove(destPath);
    }
    if (!QFile::copy(sourceFilePath, destPath)) {
        qWarning() << "录音导出失败:" << sourceFilePath << "->" << destPath;
        return false;
    }
    return true;
}

bool ExportService::exportWeeklyReport(const QString &content, const QString &filePath)
{
    if (filePath.isEmpty()) {
        qWarning() << "导出路径为空";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法写入文件:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    if (!content.endsWith("\n")) {
        out << "\n";
    }
    file.close();
    return true;
}

bool ExportService::exportNoteToPdf(const NoteData &note, const QString &filePath)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    // 使用 QPdfWriter 生成 PDF（Qt >= 5.15）
    QPdfWriter writer(filePath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setTitle(note.title);
    writer.setCreator(APP_NAME);

    QTextDocument doc;
    QString html;

    // 构建 HTML 内容
    html += "<h1>" + note.title.toHtmlEscaped() + "</h1>";
    html += "<hr>";
    html += "<p style=\"color:#999; font-size:10pt;\">";
    html += "时间: " + note.createdAt().toString("yyyy-MM-dd hh:mm:ss");
    if (!note.tag.isEmpty()) {
        html += QStringLiteral(" | 标签: ") + note.tag.toHtmlEscaped();
    }
    html += "</p><hr>";

    // 将 Markdown 内容转换为简单 HTML
    QString body = note.content.toHtmlEscaped();
    body.replace(QStringLiteral("\n"), "<br>");
    html += QStringLiteral("<p>") + body + QStringLiteral("</p>");

    doc.setHtml(html);
    doc.setPageSize(QPageSize(QPageSize::A4).size(QPageSize::Point));

    // 渲染到 PDF
    doc.print(&writer);

    qInfo() << "PDF 导出成功:" << filePath;
    return true;
#else
    Q_UNUSED(note);
    Q_UNUSED(filePath);
    qWarning() << "PDF å¯¼åºä¸æ¯æå½å Qt çæ¬";
    return false;
#endif
}
