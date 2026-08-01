// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MEETINGVIEW_H
#define MEETINGVIEW_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QListWidget>
#include <QElapsedTimer>

class AudioRecorder;

class MeetingView : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(MeetingView)

public:
    explicit MeetingView(QWidget *parent = nullptr);
    ~MeetingView() override;

signals:
    void meetingSaved(const QString &title, const QString &filePath);

private slots:
    void onStartStop();
    void onPauseResume();
    void onDurationChanged(qint64 ms);
    void onAudioLevelChanged(int level);
    void onRecordingFinished(const QString &filePath);
    void onGenerateSummary();
    void onSaveMeeting();

private:
    void initUI();
    void initConnections();
    void updateUIForState();
    QString formatDuration(qint64 ms) const;

    AudioRecorder *m_recorder;

    // UI
    QLabel *m_titleLabel;
    QLabel *m_timerLabel;
    QLabel *m_levelLabel;
    QPushButton *m_startBtn;
    QPushButton *m_pauseBtn;
    QPushButton *m_summaryBtn;
    QPushButton *m_saveBtn;
    QTextEdit *m_transcriptEdit;
    QTextEdit *m_summaryEdit;
    QListWidget *m_historyList;

    bool m_isRecording = false;
    QElapsedTimer m_elapsedTimer;
};

#endif // MEETINGVIEW_H