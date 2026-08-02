#ifndef MEETINGWIDGET_H
#define MEETINGWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QTextBrowser>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QLineEdit>
#include <QUrl>
#include <DPushButton>
#include <DLabel>
#include "storage/meetingstorage.h"

class AudioPlayer;

DWIDGET_USE_NAMESPACE

class MeetingWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(MeetingWidget)

public:
    explicit MeetingWidget(QWidget *parent = nullptr);
    void refresh();

private slots:
    void onNewMeeting();
    void onPlayPause();
    void onDeleteMeeting();
    void onAiSummary();
    void onTranscribe();
    void onSearch(const QString &keyword);
    void onTranscriptAnchorClicked(const QUrl &link);
    void onPlaybackPositionChanged(qint64 posMs);

private:
    void initUI();
    void initConnections();
    void showMeetingList();
    void showMeetingDetail(int meetingId);
    void populateMeetingList(const QList<MeetingData> &meetings);
    QString buildTranscriptHtml() const;
    void highlightTranscriptAtPosition(qint64 posMs);
    QString formatTime(qint64 ms) const;

    QStackedWidget *m_stack;
    QWidget *m_emptyPage;
    QWidget *m_listPage;
    QWidget *m_detailPage;

    QListWidget *m_meetingList;
    QLineEdit *m_searchEdit;
    QPushButton *m_newBtn;

    DLabel *m_titleLabel;
    DLabel *m_dateLabel;
    QTextEdit *m_summaryEdit;
    QTextBrowser *m_transcriptEdit;
    QPushButton *m_deleteBtn;
    QPushButton *m_backBtn;
    QPushButton *m_aiSummaryBtn;
    QPushButton *m_transcribeBtn;
    QPushButton *m_playBtn;
    QLabel *m_positionLabel;
    AudioPlayer *m_player;

    int m_currentMeetingId = -1;
    QList<TranscriptData> m_currentTranscripts;
    int m_highlightedSegmentIndex = -1;
};

#endif // MEETINGWIDGET_H
