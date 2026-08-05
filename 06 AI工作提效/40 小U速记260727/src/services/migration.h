#ifndef MIGRATION_H
#define MIGRATION_H

#include <QObject>
#include <QString>

class Database;
class NoteManager;
class TagManager;

class Migration : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Migration)

public:
    explicit Migration(Database *db, NoteManager *noteMgr, TagManager *tagMgr, QObject *parent = nullptr);

    bool hasVoiceNoteData() const;
    int importVoiceNoteData();
    int importCount() const { return m_importCount; }

    static QString voiceNoteDbPath();

signals:
    void importStarted();
    void importProgress(int current, int total);
    void importFinished(int count);

private:
    struct VNoteRecord {
        QString title;
        QString content;
        qint64 createdAt;
        qint64 modifiedAt;
        qint64 deletedAt;
        bool isDeleted;
        QString folderName;
    };

    QList<VNoteRecord> readVoiceNoteRecords() const;

    Database *m_db;
    NoteManager *m_noteManager;
    TagManager *m_tagManager;
    int m_importCount = 0;
};

#endif // MIGRATION_H