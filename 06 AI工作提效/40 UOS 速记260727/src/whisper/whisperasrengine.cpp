// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "whisperasrengine.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <cmath>

// whisper.cpp C API
#include "whisper.h"

WhisperAsrEngine::WhisperAsrEngine(const QString &modelPath, QObject *parent)
    : QObject(parent)
    , m_modelPath(modelPath)
{
    // 检查模型是否存在
    if (!QFile::exists(modelPath)) {
        qWarning() << "Whisper 模型文件不存在:" << modelPath;
    }
}

WhisperAsrEngine::~WhisperAsrEngine()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
}

void WhisperAsrEngine::transcribe(const QString &audioFile,
                                  std::function<void(const AsrResult &)> callback)
{
    if (!QFile::exists(audioFile)) {
        AsrResult result;
        result.success = false;
        result.errorMessage = tr("音频文件不存在: %1").arg(audioFile);
        callback(result);
        return;
    }

    // 在工作线程中执行转写，避免阻塞 UI
    QThread *thread = new QThread(this);
    QObject *worker = new QObject();
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [this, audioFile, callback, thread, worker]() {
        performTranscription(audioFile, callback);
        thread->quit();
    });

    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    thread->start();
}

void WhisperAsrEngine::performTranscription(const QString &audioFile,
                                            std::function<void(const AsrResult &)> callback)
{
    AsrResult result;

    // 1. 加载模型
    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;

    qInfo() << "Whisper: 加载模型..." << m_modelPath;
    struct whisper_context *ctx = whisper_init_from_file_with_params(
        m_modelPath.toUtf8().constData(), cparams);

    if (!ctx) {
        result.success = false;
        result.errorMessage = tr("模型加载失败: %1").arg(m_modelPath);
        QMetaObject::invokeMethod(QCoreApplication::instance(), [callback, result]() { callback(result); }, Qt::QueuedConnection);
        return;
    }
    m_modelLoaded = true;
    emit modelLoaded(true);

    // 2. 读取音频文件
    qInfo() << "Whisper: 读取音频..." << audioFile;

    QString convertedFile = audioFile;
    bool needsConversion = !audioFile.endsWith(".wav", Qt::CaseInsensitive);

    if (needsConversion) {
        convertedFile = QDir::tempPath() + "/whisper_temp_" + QString::number(QCoreApplication::applicationPid()) + ".wav";
        QString cmd = QString("ffmpeg -y -i \"%1\" -ar 16000 -ac 1 -sample_fmt s16 \"%2\" 2>/dev/null")
            .arg(audioFile, convertedFile);
        int ret = system(cmd.toUtf8().constData());
        if (ret != 0) {
            qWarning() << "ffmpeg 转换失败";
            whisper_free(ctx);
            result.success = false;
            result.errorMessage = tr("音频格式转换失败");
            QMetaObject::invokeMethod(QCoreApplication::instance(), [callback, result]() { callback(result); }, Qt::QueuedConnection);
            return;
        }
    }

    // 3. 读取 WAV 数据
    QFile wavFile(convertedFile);
    if (!wavFile.open(QIODevice::ReadOnly)) {
        whisper_free(ctx);
        result.success = false;
        result.errorMessage = tr("无法读取音频文件");
        QMetaObject::invokeMethod(QCoreApplication::instance(), [callback, result]() { callback(result); }, Qt::QueuedConnection);
        return;
    }

    QByteArray wavData = wavFile.readAll();
    wavFile.close();

    if (needsConversion) {
        QFile::remove(convertedFile);
    }

    if (wavData.size() < 44) {
        whisper_free(ctx);
        result.success = false;
        result.errorMessage = tr("无效的 WAV 文件");
        QMetaObject::invokeMethod(QCoreApplication::instance(), [callback, result]() { callback(result); }, Qt::QueuedConnection);
        return;
    }

    // 解析 WAV 头：采样率、位深、声道数
    // 字节偏移：22=sample_rate(4), 34=bits_per_sample(2), 22 前的 2=num_channels(2)
    int numChannels = *reinterpret_cast<const uint16_t *>(wavData.constData() + 22);
    int sampleRate = *reinterpret_cast<const uint32_t *>(wavData.constData() + 24);
    int bitsPerSample = *reinterpret_cast<const uint16_t *>(wavData.constData() + 34);
    int dataOffset = 44;
    // 兼容非标准头部：查找 "data" 块
    for (int i = 12; i < wavData.size() - 4; ++i) {
        if (memcmp(wavData.constData() + i, "data", 4) == 0) {
            dataOffset = i + 8;
            break;
        }
    }

    if (numChannels <= 0) numChannels = 1;
    if (sampleRate <= 0) sampleRate = 16000;
    if (bitsPerSample <= 0) bitsPerSample = 16;

    qInfo() << "Whisper: WAV 格式 sampleRate=" << sampleRate << " channels=" << numChannels
            << " bits=" << bitsPerSample << " dataOffset=" << dataOffset;

    // 提取 PCM 数据并转换为 float（支持 16/24/32bit，处理多声道）
    std::vector<float> pcmf32;
    pcmf32.reserve((wavData.size() - dataOffset) / 2);

    const char *pcmData = wavData.constData() + dataOffset;
    int bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample <= 0) bytesPerSample = 2;
    int totalSamples = (wavData.size() - dataOffset) / (bytesPerSample * numChannels);

    for (int i = 0; i < totalSamples; ++i) {
        const char *p = pcmData + (long)i * bytesPerSample * numChannels;
        float sample = 0.0f;
        switch (bitsPerSample) {
        case 8: {
            int8_t v = *reinterpret_cast<const int8_t *>(p);
            sample = v / 128.0f;
            break;
        }
        case 16: {
            int16_t v = *reinterpret_cast<const int16_t *>(p);
            sample = v / 32768.0f;
            break;
        }
        case 24: {
            int32_t v = (p[0] & 0xFF) | ((p[1] & 0xFF) << 8) | ((p[2] & 0xFF) << 16);
            if (v & 0x800000) v |= 0xFF000000;  // 符号扩展
            sample = v / 8388608.0f;
            break;
        }
        case 32: {
            int32_t v = *reinterpret_cast<const int32_t *>(p);
            sample = v / 2147483648.0f;
            break;
        }
        default:
            int16_t v = *reinterpret_cast<const int16_t *>(p);
            sample = v / 32768.0f;
            break;
        }
        pcmf32.push_back(sample);
    }

    // 4. 执行转写
    int nSamples = (int)pcmf32.size();
    qInfo() << "Whisper: 开始转写..." << nSamples << "samples";

    // 静音检测：计算 RMS 能量，过低则判定为静音
    if (nSamples > 0) {
        double sumSq = 0;
        for (int i = 0; i < nSamples; ++i) {
            sumSq += (double)pcmf32[i] * pcmf32[i];
        }
        double rms = std::sqrt(sumSq / nSamples);
        qInfo() << "Whisper: 音频 RMS 能量=" << rms;

        if (rms < 0.002) {
            whisper_free(ctx);
            result.success = false;
            result.errorMessage = tr("未检测到语音内容（RMS=%1，请检查麦克风或靠近麦克风说话后重试）").arg(QString::number(rms, 'f', 4));
            QMetaObject::invokeMethod(QCoreApplication::instance(), [callback, result]() { callback(result); }, Qt::QueuedConnection);
            return;
        }
    }

struct whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_realtime = false;
    wparams.print_progress = false;
    wparams.print_timestamps = false;
    wparams.print_special = false;
    wparams.translate = m_translate;
    wparams.language = "zh";              // 强制中文识别
    wparams.n_threads = m_threadCount;
    wparams.offset_ms = 0;
    wparams.no_timestamps = false;        // 输出时间戳
    wparams.single_segment = false;       // 输出多个分段
    wparams.suppress_blank = true;        // 抑制空白输出
    wparams.suppress_nst = true;          // 抑制非语音标记
    wparams.temperature = 0.0f;           // 低温度提高准确率
    wparams.temperature_inc = 0.2f;       // 逐步增加温度
    wparams.max_tokens = 128;             // 每段最大token数

    if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
        whisper_free(ctx);
        result.success = false;
        result.errorMessage = tr("转写失败");
        QMetaObject::invokeMethod(QCoreApplication::instance(), [callback, result]() { callback(result); }, Qt::QueuedConnection);
        return;
    }

    // 5. 提取转写文本（带时间戳分段）
    int nSegments = whisper_full_n_segments(ctx);
    QString fullText;

    // 过滤掉 Whisper 的特殊标记（[Music], [BLANK_AUDIO], [SIGH] 等）
    auto cleanText = [](const QString &text) -> QString {
        // 只移除 Whisper 特定的特殊标记，保留用户输入的中括号内容
        QString cleaned = text;
        cleaned.replace(QRegularExpression(R"(\[BLANK_AUDIO\])"), "");
        cleaned.replace(QRegularExpression(R"(\[Music\])"), "");
        cleaned.replace(QRegularExpression(R"(\[SIGH\])"), "");
        cleaned.replace(QRegularExpression(R"(\[LAUGH\])"), "");
        cleaned.replace(QRegularExpression(R"(\[NOISE\])"), "");
        cleaned.replace(QRegularExpression(R"(\[SPEECH\])"), "");
        return cleaned.trimmed();
    };

    // 说话人识别：基于每个分段的声学特征（能量 + 过零率）聚类
    struct SegFeature {
        int index;
        float energy;
        float zcr;
        int speaker = 0;
    };
    QList<SegFeature> features;

    for (int i = 0; i < nSegments; ++i) {
        const char *text = whisper_full_get_segment_text(ctx, i);
        if (!text) continue;
        QString segText = cleanText(QString::fromUtf8(text));
        if (segText.isEmpty()) continue;

        AsrSegment seg;
        seg.text = segText;
        seg.startMs = whisper_full_get_segment_t0(ctx, i) * 10;
        seg.endMs = whisper_full_get_segment_t1(ctx, i) * 10;
        result.segments.append(seg);

        if (!fullText.isEmpty()) fullText += " ";
        fullText += segText;

        // 计算该分段对应的声学特征
        int startSample = (int)(seg.startMs / 1000.0 * 16000.0);
        int endSample = (int)(seg.endMs / 1000.0 * 16000.0);
        if (startSample < 0) startSample = 0;
        if (endSample > nSamples) endSample = nSamples;

        float energy = 0.0f;
        float zcr = 0.0f;
        if (endSample > startSample) {
            int count = endSample - startSample;
            for (int s = startSample; s < endSample; ++s) {
                float v = pcmf32[s];
                energy += v * v;
                if (s > startSample && ((pcmf32[s] >= 0) != (pcmf32[s-1] >= 0))) {
                    zcr += 1.0f;
                }
            }
            energy = std::sqrt(energy / count);
            zcr = zcr / count;
        }

        SegFeature f;
        f.index = i;
        f.energy = energy;
        f.zcr = zcr;
        features.append(f);
    }

    // 简单的声学特征聚类：相邻分段特征差异大则切换说话人
    if (!features.isEmpty()) {
        int speakerId = 1;
        features[0].speaker = speakerId;
        for (int k = 1; k < features.size(); ++k) {
            float de = std::fabs(features[k].energy - features[k-1].energy);
            float dz = std::fabs(features[k].zcr - features[k-1].zcr);
            // 能量差 > 0.015 或过零率差 > 0.01 认为切换说话人
            if (de > 0.015f || dz > 0.01f) {
                speakerId = (speakerId % 2) + 1;  // 在说话人1/2之间切换
            }
            features[k].speaker = speakerId;
        }

        // 将说话人标签写回分段
        for (int k = 0; k < features.size() && k < result.segments.size(); ++k) {
            result.segments[k].speaker = QString("说话人%1").arg(features[k].speaker);
        }
    }

    // 6. 清理并返回结果
    whisper_free(ctx);

    result.success = true;
    result.text = fullText.trimmed();
    qInfo() << "Whisper: 转写完成, 文本长度:" << result.text.length();

    QMetaObject::invokeMethod(QCoreApplication::instance(), [callback, result]() { callback(result); }, Qt::QueuedConnection);
}

QString WhisperAsrEngine::modelSizeName() const
{
    if (m_modelPath.contains("tiny")) return "Tiny";
    if (m_modelPath.contains("base")) return "Base";
    if (m_modelPath.contains("small")) return "Small";
    if (m_modelPath.contains("medium")) return "Medium";
    if (m_modelPath.contains("large")) return "Large";
    return "Unknown";
}

QString WhisperAsrEngine::defaultModelPath(ModelSize size)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                  + "/whisper-models";
    QDir().mkpath(dir);

    switch (size) {
    case Tiny:   return dir + "/ggml-tiny.bin";
    case Base:   return dir + "/ggml-base.bin";
    case Small:  return dir + "/ggml-small.bin";
    case Medium: return dir + "/ggml-medium.bin";
    case Large:  return dir + "/ggml-large-v3.bin";
    }
    return dir + "/ggml-tiny.bin";
}

QString WhisperAsrEngine::modelDownloadUrl(ModelSize size)
{
    QString model;
    switch (size) {
    case Tiny:   model = "ggml-tiny.bin"; break;
    case Base:   model = "ggml-base.bin"; break;
    case Small:  model = "ggml-small.bin"; break;
    case Medium: model = "ggml-medium.bin"; break;
    case Large:  model = "ggml-large-v3.bin"; break;
    }
    return "https://hf-mirror.com/ggerganov/whisper.cpp/resolve/main/" + model;
}

void WhisperAsrEngine::testConnection(std::function<void(bool success, const QString &message)> callback)
{
    if (QFile::exists(m_modelPath)) {
        callback(true, QStringLiteral("Whisper 模型文件已就绪"));
    } else {
        callback(false, QStringLiteral("Whisper 模型文件不存在: ") + m_modelPath);
    }
}
