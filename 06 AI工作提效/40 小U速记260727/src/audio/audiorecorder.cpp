// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audiorecorder.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <cmath>

// GStreamer 初始化（全局一次）
static bool s_gstInitialized = false;
static void ensureGstInit()
{
    if (!s_gstInitialized) {
        gst_init(nullptr, nullptr);
        s_gstInitialized = true;
    }
}

QString AudioRecorder::recordingDir()
{
    // 优先使用设置页配置的存储目录
    const QString configured = QSettings().value("recording/storage_dir").toString().trimmed();
    if (!configured.isEmpty()) {
        return configured;
    }
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/UOS速记/录音";
}

QString AudioRecorder::makeRecordingPath(const QString &prefix)
{
    QString dir = recordingDir();
    QDir().mkpath(dir);
    return dir + "/" + prefix + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".wav";
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
        path = makeRecordingPath("录音_");
    }
    m_filePath = path;

    // 确保目标目录存在
    QDir().mkpath(QFileInfo(path).absolutePath());

    // 构建 GStreamer 管道（参考系统语音记事本 deepin-voice-note 的录音链路优化）：
    //
    // autoaudiosrc
    //   → audioconvert      格式转换
    //   → audioresample     重采样至 16000Hz
    //   → audiorate         校准采样率，保证输出严格 16000Hz（ASR 依赖精确采样率）
    //   → audiochebband     80–4000Hz 带通滤波，滤除低频噪声/高频杂音，突出人声
    //   → volume            增益 1.0x（TC19：原 1.5x 会放大麦克风底噪，导致「沙沙声」明显）
    //   → audiodynamic      扩展器（mode=2 expander，低于阈值的安静片段自动压低），
    //                       抑制说话间隙的底噪/沙沙声；参数沿用已验证可解析的 threshold/ratio
    //   → audioconvert      转换回 S16LE
    //   → capsfilter        设置格式: 16000Hz, mono, S16LE（与 ASR 引擎一致）
    //   → level             实时电平检测（替换原来的假电平）
    //   → wavenc            WAV 编码
    //   → filesink          文件输出
    //
    // 注意：filesink 的 location 不嵌入管道字符串，避免路径含空格/中文/特殊字符时解析失败。
    QString pipelineStr = QString(
        "autoaudiosrc name=src ! "
        "audioconvert ! audioresample ! audiorate ! "
        "audiochebband name=band mode=1 lower-frequency=80 upper-frequency=4000 ! "
        "volume name=volume volume=1.0 ! "
        "audiodynamic name=dynamic mode=expander threshold=0.25 ratio=3.0 ! "
        "audioconvert ! "
        "audio/x-raw, format=S16LE, rate=16000, channels=1 ! "
        "level name=level interval=200000000 ! "
        "wavenc ! "
        "filesink name=file_sink"
    );

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
    m_volume = gst_bin_get_by_name(GST_BIN(m_pipeline), "volume");
    m_level = gst_bin_get_by_name(GST_BIN(m_pipeline), "level");
    m_sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "file_sink");
    if (m_sink) {
        g_object_set(m_sink, "location", path.toUtf8().constData(), nullptr);
    } else {
        qWarning() << "录音管道缺少 filesink 元素";
        // 清理已获取的子元素引用，避免泄漏
        if (m_source) { gst_object_unref(m_source); m_source = nullptr; }
        if (m_volume) { gst_object_unref(m_volume); m_volume = nullptr; }
        if (m_level)  { gst_object_unref(m_level);  m_level = nullptr; }
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        emit errorOccurred("创建录音管道失败: 缺少 filesink 元素");
        return false;
    }

    // 监听总线消息
    GstBus *bus = gst_element_get_bus(m_pipeline);
    gst_bus_add_watch(bus, &AudioRecorder::onBusMessage, this);
    gst_object_unref(bus);

    // 开始录制
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        qWarning() << "录音启动失败";
        if (m_source) { gst_object_unref(m_source); m_source = nullptr; }
        if (m_volume) { gst_object_unref(m_volume); m_volume = nullptr; }
        if (m_level)  { gst_object_unref(m_level);  m_level = nullptr; }
        if (m_sink)   { gst_object_unref(m_sink);   m_sink = nullptr; }
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        emit errorOccurred("录音启动失败");
        return false;
    }

    m_state = Recording;
    m_durationMs = 0;
    m_audioLevel = 0;
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
        // 发送 EOS 事件，确保 wavenc 写入完整的 WAV 文件头
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
        // 先释放子元素引用（gst_bin_get_by_name 会 +1 ref），再释放管道本身，
        // 否则子元素残留引用指向已释放管道 → 第二次录音时 use-after-free 闪退（TC19）
        if (m_source) { gst_object_unref(m_source); m_source = nullptr; }
        if (m_volume) { gst_object_unref(m_volume); m_volume = nullptr; }
        if (m_level)  { gst_object_unref(m_level);  m_level = nullptr; }
        if (m_sink)   { gst_object_unref(m_sink);   m_sink = nullptr; }
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }

    m_state = Idle;
    emit stateChanged(m_state);
    emit recordingFinished(m_filePath);
    qInfo() << "录音结束:" << m_filePath << "时长:" << m_durationMs << "ms";

    // 校验录音文件是否已正确落盘（存在且非空）
    QFileInfo fi(m_filePath);
    if (!fi.exists() || fi.size() <= 0) {
        qWarning() << "录音文件缺失或为空:" << m_filePath;
        emit errorOccurred("录音文件未正确保存，请重新录音");
    }
    return true;
}

void AudioRecorder::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event)
    updateDuration();

    // 从 GStreamer level 元素获取实时音频电平
    if (m_level) {
        GstStructure *s = nullptr;
        g_signal_emit_by_name(m_level, "get-level", (gdouble)1.0, 0.0, &s);
        if (s) {
            gdouble peak_dB;
            if (gst_structure_get_double(s, "peak", &peak_dB)) {
                // 将 dB 值映射到 0-100 范围
                // 通常语音峰值在 -30dB ~ -6dB 之间
                // -60dB 以下视为静音（0%），0dB 为最大（100%）
                const double minDb = -60.0;
                const double maxDb = -3.0;
                double normalized = (peak_dB - minDb) / (maxDb - minDb);
                m_audioLevel = qBound(0, static_cast<int>(normalized * 100.0), 100);
            }
            gst_structure_free(s);
        }
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
