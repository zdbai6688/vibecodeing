#ifndef MEETINGWIDGET_H
#define MEETINGWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QTextBrowser>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QLineEdit>
#include <QTimer>
#include <QUrl>
#include <DPushButton>
#include <DLabel>
#include "storage/meetingstorage.h"

class AudioPlayer;
class AudioRecorder;

DWIDGET_USE_NAMESPACE

class MeetingWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(MeetingWidget)

public:
    explicit MeetingWidget(QWidget *parent = nullptr);
    void refresh();

public slots:
    void onNewMeeting();

private slots:
    void onStartRecording();
    void onStopRecording(const QString &filePath = QString());
    void onDeleteMeeting();
    void onAiSummary();
    void onTranscribe();
    void onTogglePlayback();
    void onSearch(const QString &keyword);

private:
    void initUI();
    void initConnections();
    void showMeetingList();
    void showMeetingDetail(int meetingId);
    void populateMeetingList(const QList<MeetingData> &meetings);
    void populateHistoryList(const QList<MeetingData> &meetings);
    // 批量操作
    void enterMultiSelectMode();
    void exitMultiSelectMode();
    void updateSelectionState();
    void onBatchDelete();
    QList<int> getSelectedMeetingIds() const;
    QString buildTranscriptHtml(int highlightIndex = -1) const;
    QString formatTime(qint64 ms) const;

    QStackedWidget *m_stack;
    QWidget *m_emptyPage;
    QWidget *m_listPage;
    QWidget *m_detailPage;

    // 右侧历史会议清单栏（TC07 ②：保留原设计，最右侧新增历史清单，切分为三块布局）
    QWidget *m_historyPanel;
    QListWidget *m_historyList;
    QLineEdit *m_historySearch;
    QLabel *m_historyCountLabel;

    QListWidget *m_meetingList;
    QLineEdit *m_searchEdit;
    QPushButton *m_newBtn;

    // 批量操作
    QPushButton *m_selectModeBtn;
    bool m_multiSelectMode = false;
    QWidget *m_batchToolbar;
    QPushButton *m_selectAllBtn;
    QPushButton *m_batchDeleteBtn;
    DLabel *m_selectionCountLabel;

    DLabel *m_titleLabel;
    DLabel *m_dateLabel;
    QTextEdit *m_summaryEdit;
    QTextBrowser *m_transcriptEdit;
    QPushButton *m_deleteBtn;
    QPushButton *m_backBtn;
    QPushButton *m_aiSummaryBtn;
    QPushButton *m_transcribeBtn;
    QLabel *m_fileLabel;
    QPushButton *m_exportBtn;
    QPushButton *m_playBtn;
    AudioPlayer *m_player;
    AudioRecorder *m_recorder;
    QPushButton *m_recordingBtn;
    QTimer *m_recordingAnimTimer;
    int m_recordingDotCount = 0;
    qint64 m_recordingStartTime = 0;

    int m_currentMeetingId = -1;
    QString m_currentAudioFilePath;
    QList<TranscriptData> m_currentTranscripts;
    int m_highlightedSegmentIndex = -1;
    bool m_searching = false;   // 搜索态：列表为空时显示「无匹配」，不跳回主页（TC20）
};

#endif // MEETINGWIDGET_H
