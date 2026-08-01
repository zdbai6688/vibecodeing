// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audiorecorder.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <gst/gst.h>

// GStreamer 初始化（全局一次）
static bool s_gstInitialized = false;
static void ensureGstInit()
{
    if (!s_gstInitialized) {
        gst_init(nullptr, nullptr);
        s_gstInitialized = true;
    }
}

AudioRecorder::AudioRecorder(QObject *parent)
    : QObject(parent)
{
    ensureGstInit();
}

AudioRecorder::~AudioRecorder()
{
    if (m_state != Idle) {
        stopRecording();
    }
    if (m_pipeline) {
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
}

bool AudioRecorder::startRecording(const QString &filePath)
{
    if (m_state != Idle) return false;

    // 确定保存路径
    QString path = filePath;
    if (path.isEmpty()) {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                      + "/UOS速记/录音";
        QDir().mkpath(dir);
        path = dir + "/录音_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".wav";
    }
    m_filePath = path;

    // 构建 GStreamer 管道:
    // autoaudiosrc ! audioconvert ! audioresample ! capsfilter(S16LE 16kHz mono) ! wavenc ! filesink
    QString pipelineStr = QString(
        "autoaudiosrc name=src ! "
        "audioconvert ! audioresample ! "
        "capsfilter caps=\"audio/x-raw, format=S16LE, rate=16000, channels=1\" ! "
        "wavenc ! "
        "filesink name=file_sink location=\"%1\""
    ).arg(path);

    GError *error = nullptr;
    m_pipeline = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);
    if (!m_pipeline) {
        QString errMsg = error ? error->message : "未知错误";
        qWarning() << "创建录音管道失败:" << errMsg;
        if (error) g_error_free(error);
        emit errorOccurred("创建录音管道失败: " + errMsg);
        return false;
    }

    m_source = gst_bin_get_by_name(GST_BIN(m_pipeline), "src");
    m_sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "file_sink");

    // 监听总线消息
    GstBus *bus = gst_element_get_bus(m_pipeline);
    gst_bus_add_watch(bus, &AudioRecorder::onBusMessage, this);
    gst_object_unref(bus);

    // 开始录制
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        qWarning() << "录音启动失败";
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        emit errorOccurred("录音启动失败");
        return false;
    }

    m_state = Recording;
    m_durationMs = 0;
    m_startTime = QDateTime::currentMSecsSinceEpoch();
    m_timerId = startTimer(250); // 250ms 更新时长和电平

    emit stateChanged(m_state);
    qInfo() << "录音开始:" << m_filePath;
    return true;
}

bool AudioRecorder::pauseRecording()
{
    if (m_state != Recording) return false;

    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    }
    m_state = Paused;
    if (m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    emit stateChanged(m_state);
    return true;
}

bool AudioRecorder::resumeRecording()
{
    if (m_state != Paused) return false;

    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    }
    m_state = Recording;
    m_timerId = startTimer(250);
    emit stateChanged(m_state);
    return true;
}

bool AudioRecorder::stopRecording()
{
    if (m_state == Idle) return false;

    if (m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
    }

    if (m_pipeline) {
        // 发送 EOS 事件
        gst_element_send_event(m_pipeline, gst_event_new_eos());
        // 等待 EOS 或超时
        GstBus *bus = gst_element_get_bus(m_pipeline);
        if (bus) {
            GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_SECOND * 2,
                (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
            if (msg) gst_message_unref(msg);
            gst_object_unref(bus);
        }
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        m_source = nullptr;
        m_sink = nullptr;
    }

    m_state = Idle;
    emit stateChanged(m_state);
    emit recordingFinished(m_filePath);
    qInfo() << "录音结束:" << m_filePath << "时长:" << m_durationMs << "ms";
    return true;
}

void AudioRecorder::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event)
    updateDuration();

    // 获取音频电平（简化实现）
    if (m_pipeline) {
        // 可以通过 level 元素获取更精确的电平，这里简化处理
        m_audioLevel = (m_audioLevel + 30) % 100;
        emit audioLevelChanged(m_audioLevel);
    }
}

void AudioRecorder::updateDuration()
{
    if (m_startTime > 0) {
        m_durationMs = QDateTime::currentMSecsSinceEpoch() - m_startTime;
        emit durationChanged(m_durationMs);
    }
}

gboolean AudioRecorder::onBusMessage(GstBus *bus, GstMessage *msg, gpointer userData)
{
    Q_UNUSED(bus)
    auto *recorder = static_cast<AudioRecorder *>(userData);

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError *err = nullptr;
        gchar *debug = nullptr;
        gst_message_parse_error(msg, &err, &debug);
        QString errMsg = err ? err->message : "未知错误";
        qWarning() << "录音错误:" << errMsg;
        if (err) g_error_free(err);
        if (debug) g_free(debug);
        emit recorder->errorOccurred(errMsg);
        recorder->stopRecording();
        break;
    }
    case GST_MESSAGE_EOS:
        qInfo() << "录音管道 EOS";
        break;
    case GST_MESSAGE_WARNING: {
        GError *err = nullptr;
        gchar *debug = nullptr;
        gst_message_parse_warning(msg, &err, &debug);
        if (err) g_error_free(err);
        if (debug) g_free(debug);
        break;
    }
    default:
        break;
    }
    return TRUE;
}