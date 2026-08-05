// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WHISPERASRENGINE_H
#define WHISPERASRENGINE_H

#include <QObject>
#include <QString>
#include <QThread>
#include <QMutex>
#include <functional>
#include <atomic>

#include "services/asrservice.h"

/**
 * Whisper.cpp 离线语音识别引擎
 * 
 * 完全本地运行，无需网络连接
 * 支持多语言，中文识别精度优秀
 * 使用 tiny 模型（~75MB）或 larger 模型（base/small/medium/large）
 */
class WhisperAsrEngine : public QObject, public IAsrEngine
{
    Q_OBJECT
    Q_DISABLE_COPY(WhisperAsrEngine)

public:
    enum ModelSize {
        Tiny,   // ~75MB, 最快
        Base,   // ~150MB
        Small,  // ~500MB
        Medium, // ~1.5GB
        Large   // ~3GB
    };

    explicit WhisperAsrEngine(const QString &modelPath, QObject *parent = nullptr);
    ~WhisperAsrEngine() override;

    QString name() const override { return tr("离线语音 (Whisper)"); }
    void transcribe(const QString &audioFile,
                    std::function<void(const AsrResult &)> callback) override;

    // 模型管理
    bool isModelLoaded() const { return m_modelLoaded; }
    QString modelPath() const { return m_modelPath; }
    QString modelSizeName() const;
    static QString defaultModelPath(ModelSize size = Tiny);
    static QString modelDownloadUrl(ModelSize size);

    // 设置
    void setLanguage(const QString &lang) { m_language = lang; }
    void setThreadCount(int n) { m_threadCount = n; }
    void setTranslate(bool translate) { m_translate = translate; }

    // 测试连接：检查模型文件是否存在
    void testConnection(std::function<void(bool success, const QString &message)> callback) override;

signals:
    void modelLoadProgress(int percent);
    void transcriptionProgress(const QString &text, bool isPartial);
    void modelLoaded(bool success);

private:
    void performTranscription(const QString &audioFile,
                              std::function<void(const AsrResult &)> callback);

    QString m_modelPath;
    QString m_language = "zh";
    int m_threadCount = 4;
    bool m_translate = false;
    std::atomic<bool> m_modelLoaded{false};

    // 工作线程
    QThread *m_workerThread = nullptr;
    mutable QMutex m_mutex;
};

#endif // WHISPERASRENGINE_H
