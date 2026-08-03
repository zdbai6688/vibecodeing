// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUDIORECORDER_H
#define AUDIORECORDER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <gst/gst.h>

/**
 * 基于 GStreamer 的音频录制器
 * 支持：开始/暂停/继续/停止录音，实时音频电平，录音时长
 */
class AudioRecorder : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(AudioRecorder)

public:
    enum State {
        Idle,
        Recording,
        Paused
    };
    Q_ENUM(State)

    explicit AudioRecorder(QObject *parent = nullptr);
    ~AudioRecorder() override;

    bool startRecording(const QString &filePath = QString());
    bool pauseRecording();
    bool resumeRecording();
    bool stopRecording();

    State state() const { return m_state; }
    QString currentFilePath() const { return m_filePath; }
    qint64 durationMs() const { return m_durationMs; }
    int audioLevel() const { return m_audioLevel; }  // 0-100

    // 录音存储目录：优先读取设置页配置（recording/storage_dir），未配置时返回默认目录
    static QString recordingDir();
    // 在录音存储目录下生成带时间戳的录音文件路径
    static QString makeRecordingPath(const QString &prefix);

signals:
    void stateChanged(AudioRecorder::State newState);
    void durationChanged(qint64 ms);
    void audioLevelChanged(int level);  // 0-100
    void recordingFinished(const QString &filePath);
    void errorOccurred(const QString &message);

private:
    static gboolean onBusMessage(GstBus *bus, GstMessage *msg, gpointer userData);
    static gboolean onTimerTick(gpointer userData);
    void updateDuration();
    void timerEvent(QTimerEvent *event) override;

    State m_state = Idle;
    QString m_filePath;
    qint64 m_durationMs = 0;
    int m_audioLevel = 0;

    // GStreamer
    GstElement *m_pipeline = nullptr;
    GstElement *m_source = nullptr;
    GstElement *m_encoder = nullptr;
    GstElement *m_sink = nullptr;

    int m_timerId = 0;
    qint64 m_startTime = 0;
};

#endif // AUDIORECORDER_H
