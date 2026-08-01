// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "meetingview.h"
#include "audio/audiorecorder.h"
#include "services/aiservice.h"
#include "application/shorthandapplication.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <DStyle>
#include <DLabel>
#include <DFontSizeManager>
#include <DGuiApplicationHelper>
#include <QDebug>

MeetingView::MeetingView(QWidget *parent)
    : QWidget(parent)
{
    m_recorder = new AudioRecorder(this);
    initUI();
    initConnections();
}

MeetingView::~MeetingView()
{
    if (m_recorder->state() != AudioRecorder::Idle) {
        m_recorder->stopRecording();
    }
}

void MeetingView::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 8, 16, 8);
    mainLayout->setSpacing(8);

    // 标题
    m_titleLabel = new DLabel(tr("🎤 会议速记"), this);
    DFontSizeManager::instance()->bind(m_titleLabel, DFontSizeManager::T5, QFont::DemiBold);
    mainLayout->addWidget(m_titleLabel);

    // 录音控制区
    QHBoxLayout *controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(8);

    m_timerLabel = new QLabel(tr("00:00:00"), this);
    m_timerLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #E02020;");
    m_timerLabel->setFixedWidth(180);

    m_levelLabel = new QLabel(tr("电平: --"), this);
    m_levelLabel->setFixedWidth(80);

    m_startBtn = new QPushButton(tr("▶ 开始录音"), this);
    m_startBtn->setFixedHeight(40);
    m_startBtn->setMinimumWidth(120);
    // 录音按钮保持红色作为警示色（语义颜色不随主题变化）
    m_startBtn->setStyleSheet("QPushButton { background-color: #E02020; color: white; border-radius: 6px; font-size: 14px; }");

    m_pauseBtn = new QPushButton(tr("⏸ 暂停"), this);
    m_pauseBtn->setFixedHeight(40);
    m_pauseBtn->setMinimumWidth(80);
    m_pauseBtn->setEnabled(false);

    m_summaryBtn = new QPushButton(tr("🤖 AI 摘要"), this);
    m_summaryBtn->setFixedHeight(40);
    m_summaryBtn->setMinimumWidth(100);
    m_summaryBtn->setEnabled(false);

    m_saveBtn = new QPushButton(tr("💾 保存"), this);
    m_saveBtn->setFixedHeight(40);
    m_saveBtn->setMinimumWidth(80);
    m_saveBtn->setEnabled(false);

    controlLayout->addWidget(m_timerLabel);
    controlLayout->addWidget(m_levelLabel);
    controlLayout->addStretch();
    controlLayout->addWidget(m_startBtn);
    controlLayout->addWidget(m_pauseBtn);
    controlLayout->addWidget(m_summaryBtn);
    controlLayout->addWidget(m_saveBtn);

    mainLayout->addLayout(controlLayout);

    // 内容区：转写 + 摘要
    QHBoxLayout *contentLayout = new QHBoxLayout();

    // 左侧：转写内容
    QGroupBox *transcriptGroup = new QGroupBox(tr("转写内容"), this);
    QVBoxLayout *transLayout = new QVBoxLayout(transcriptGroup);
    m_transcriptEdit = new QTextEdit(this);
    m_transcriptEdit->setPlaceholderText(tr("录音内容将实时显示在这里...\n\n"
        "说明：当前版本使用纯录音模式\n"
        "实时语音转写将在 Phase 3 集成 Whisper.cpp 后启用"));
    m_transcriptEdit->setReadOnly(true);
    transLayout->addWidget(m_transcriptEdit);
    contentLayout->addWidget(transcriptGroup, 1);

    // 右侧：AI 摘要
    QGroupBox *summaryGroup = new QGroupBox(tr("AI 会议纪要"), this);
    QVBoxLayout *sumLayout = new QVBoxLayout(summaryGroup);
    m_summaryEdit = new QTextEdit(this);
    m_summaryEdit->setPlaceholderText(tr("点击「AI 摘要」生成会议纪要..."));
    m_summaryEdit->setReadOnly(true);
    sumLayout->addWidget(m_summaryEdit);
    contentLayout->addWidget(summaryGroup, 1);

    mainLayout->addLayout(contentLayout, 1);

    // 历史记录
    QGroupBox *historyGroup = new QGroupBox(tr("会议历史"), this);
    QVBoxLayout *histLayout = new QVBoxLayout(historyGroup);
    m_historyList = new QListWidget(this);
    m_historyList->setMaximumHeight(120);
    m_historyList->setFrameShape(QFrame::NoFrame);
    histLayout->addWidget(m_historyList);
    mainLayout->addWidget(historyGroup);

    updateUIForState();
}

void MeetingView::initConnections()
{
    // 录音控制
    connect(m_startBtn, &QPushButton::clicked, this, &MeetingView::onStartStop);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MeetingView::onPauseResume);
    connect(m_summaryBtn, &QPushButton::clicked, this, &MeetingView::onGenerateSummary);
    connect(m_saveBtn, &QPushButton::clicked, this, &MeetingView::onSaveMeeting);

    // 录音状态
    connect(m_recorder, &AudioRecorder::durationChanged, this, &MeetingView::onDurationChanged);
    connect(m_recorder, &AudioRecorder::audioLevelChanged, this, &MeetingView::onAudioLevelChanged);
    connect(m_recorder, &AudioRecorder::recordingFinished, this, &MeetingView::onRecordingFinished);
    connect(m_recorder, &AudioRecorder::stateChanged, this, [this]() {
        updateUIForState();
    });
    connect(m_recorder, &AudioRecorder::errorOccurred, this, [this](const QString &msg) {
        m_transcriptEdit->append(tr("⚠️ 录音错误: %1").arg(msg));
    });
}

void MeetingView::onStartStop()
{
    if (m_recorder->state() == AudioRecorder::Idle) {
        // 开始录音
        m_transcriptEdit->clear();
        m_summaryEdit->clear();
        m_transcriptEdit->append(tr("🎙️ 录音开始...\n"));
        m_recorder->startRecording();
        m_elapsedTimer.start();
    } else {
        // 停止录音
        m_recorder->stopRecording();
    }
}

void MeetingView::onPauseResume()
{
    if (m_recorder->state() == AudioRecorder::Recording) {
        m_recorder->pauseRecording();
        m_transcriptEdit->append(tr("⏸ 录音已暂停\n"));
    } else if (m_recorder->state() == AudioRecorder::Paused) {
        m_recorder->resumeRecording();
        m_transcriptEdit->append(tr("▶ 继续录音\n"));
    }
}

void MeetingView::onDurationChanged(qint64 ms)
{
    m_timerLabel->setText(formatDuration(ms));
}

void MeetingView::onAudioLevelChanged(int level)
{
    QString bar;
    int count = level / 10;
    for (int i = 0; i < count; ++i) bar += "█";
    m_levelLabel->setText(QString("电平: %1%").arg(level));
}

void MeetingView::onRecordingFinished(const QString &filePath)
{
    m_transcriptEdit->append(tr("\n✅ 录音完成"));
    m_transcriptEdit->append(tr("📁 文件: %1").arg(filePath));
    m_transcriptEdit->append(tr("⏱ 时长: %1").arg(formatDuration(m_recorder->durationMs())));

    // 启用摘要和保存按钮
    m_summaryBtn->setEnabled(true);
    m_saveBtn->setEnabled(true);

    // 添加到历史列表
    QDateTime now = QDateTime::currentDateTime();
    QString title = tr("会议录音 %1").arg(now.toString("MM-dd hh:mm"));
    m_historyList->insertItem(0, title + "  |  " + formatDuration(m_recorder->durationMs()));
}

void MeetingView::onGenerateSummary()
{
    m_summaryBtn->setEnabled(false);
    m_summaryEdit->setPlainText(tr("🤖 AI 正在生成摘要...\n\n"
        "请先在设置中配置 AI API Key\n"
        "支持的引擎: DeepSeek / 通义千问 / OpenAI"));

    // 获取已保存的录音内容（暂为模拟，Phase 3 接入 ASR 后替换）
    QString transcriptText = m_transcriptEdit->toPlainText();
    if (transcriptText.length() < 20) {
        m_summaryEdit->setPlainText(tr("⚠️ 录音内容为空或过短，无法生成摘要"));
        m_summaryBtn->setEnabled(true);
        return;
    }

    // 使用 AI 服务（实际部署时取消注释）
    /*
    auto *app = ShorthandApplication::instance();
    AIService *ai = new AIService(this);
    connect(ai, &AIService::summaryReady, this, [this](const QString &summary) {
        m_summaryEdit->setPlainText(summary);
        m_summaryBtn->setEnabled(true);
    });
    connect(ai, &AIService::errorOccurred, this, [this](const QString &err) {
        m_summaryEdit->setPlainText(tr("❌ %1").arg(err));
        m_summaryBtn->setEnabled(true);
    });
    ai->generateSummary(transcriptText);
    */
}

void MeetingView::onSaveMeeting()
{
    QString title = tr("会议记录 %1").arg(
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"));
    emit meetingSaved(title, m_recorder->currentFilePath());
    m_saveBtn->setEnabled(false);
}

void MeetingView::updateUIForState()
{
    AudioRecorder::State state = m_recorder->state();
    m_isRecording = (state != AudioRecorder::Idle);

    switch (state) {
    case AudioRecorder::Idle:
        m_startBtn->setText(tr("▶ 开始录音"));
        m_startBtn->setStyleSheet("QPushButton { background-color: #E02020; color: white; border-radius: 6px; font-size: 14px; }");
        m_pauseBtn->setEnabled(false);
        m_pauseBtn->setText(tr("⏸ 暂停"));
        break;
    case AudioRecorder::Recording:
        m_startBtn->setText(tr("⏹ 停止录音"));
        m_startBtn->setStyleSheet("QPushButton { background-color: palette(mid); color: palette(windowText); border-radius: 6px; font-size: 14px; }");
        m_pauseBtn->setEnabled(true);
        m_pauseBtn->setText(tr("⏸ 暂停"));
        break;
    case AudioRecorder::Paused:
        m_startBtn->setText(tr("⏹ 停止录音"));
        m_pauseBtn->setEnabled(true);
        m_pauseBtn->setText(tr("▶ 继续"));
        break;
    }
}

QString MeetingView::formatDuration(qint64 ms) const
{
    int secs = ms / 1000;
    int hours = secs / 3600;
    int mins = (secs % 3600) / 60;
    int secs2 = secs % 60;
    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs2, 2, 10, QChar('0'));
}
