// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "meetingwidget.h"
#include "application/shorthandapplication.h"
#include "core/meetingmanager.h"
#include "services/aiservice.h"
#include "services/asrservice.h"
#include "audio/audioplayer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QFile>
#include <QInputDialog>
#include <DLabel>
#include <DFontSizeManager>
#include <DDialog>
#include <DGuiApplicationHelper>
#include <QDebug>

static void stylePrimaryBtn(QPushButton *btn)
{
    btn->setFixedHeight(36);
    btn->setStyleSheet(R"(
        QPushButton {
            background: palette(highlight); color: palette(highlightedText); border: none;
            border-radius: 6px; padding: 4px 24px; font-size: 13px; font-weight: 600;
        }
        QPushButton:hover { background: palette(dark); }
        QPushButton:disabled { background: palette(mid); color: palette(windowText); }
    )");
}

MeetingWidget::MeetingWidget(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("MeetingWidget { background: palette(base); }");
    m_player = new AudioPlayer(this);
    initUI();
    initConnections();
}

void MeetingWidget::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);

    // 空状态页
    m_emptyPage = new QWidget(this);
    QVBoxLayout *emptyLayout = new QVBoxLayout(m_emptyPage);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(12);

    DLabel *emptyIcon = new DLabel(tr("🎤"), this);
    emptyIcon->setStyleSheet("font-size: 64px;");
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyIcon);

    DLabel *emptyTitle = new DLabel(tr("会议记录"), this);
    emptyTitle->setStyleSheet("font-size: 16px; font-weight: 600;");
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);

    DLabel *emptyDesc = new DLabel(tr("记录会议音频，支持语音转文字和 AI 摘要"), this);
    emptyDesc->setStyleSheet("font-size: 13px; color: palette(placeholderText);");
    emptyDesc->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyDesc);

    QPushButton *startBtn = new QPushButton(tr("🎤 开始记录"), this);
    stylePrimaryBtn(startBtn);
    emptyLayout->addWidget(startBtn, 0, Qt::AlignCenter);
    connect(startBtn, &QPushButton::clicked, this, &MeetingWidget::onNewMeeting);

    m_stack->addWidget(m_emptyPage);

    // 列表页
    m_listPage = new QWidget(this);
    QVBoxLayout *listLayout = new QVBoxLayout(m_listPage);
    listLayout->setContentsMargins(16, 12, 16, 12);
    listLayout->setSpacing(8);

    QHBoxLayout *headerRow = new QHBoxLayout();
    DLabel *listTitle = new DLabel(tr("会议记录"), this);
    listTitle->setStyleSheet("font-size: 16px; font-weight: 600;");
    headerRow->addWidget(listTitle);
    headerRow->addStretch();
    m_newBtn = new QPushButton(tr("＋ 新建"), this);
    stylePrimaryBtn(m_newBtn);
    m_newBtn->setFixedHeight(30);
    headerRow->addWidget(m_newBtn);
    listLayout->addLayout(headerRow);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("搜索会议..."));
    m_searchEdit->setFixedHeight(32);
    m_searchEdit->setStyleSheet("QLineEdit { border:1px solid palette(mid); border-radius:6px; padding:4px 12px; font-size:13px; background:palette(window); } QLineEdit:focus { border-color:palette(highlight); }");
    listLayout->addWidget(m_searchEdit);

    m_meetingList = new QListWidget(this);
    m_meetingList->setFrameShape(QFrame::NoFrame);
    m_meetingList->setSpacing(4);
    m_meetingList->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; }
        QListWidget::item {
            border-radius: 6px; padding: 12px 16px; margin: 2px 0;
            background: palette(base); border: 1px solid palette(midlight);
        }
        QListWidget::item:hover { background: palette(light); }
    )");
    listLayout->addWidget(m_meetingList, 1);
    m_stack->addWidget(m_listPage);

    // 详情页
    m_detailPage = new QWidget(this);
    QVBoxLayout *detailLayout = new QVBoxLayout(m_detailPage);
    detailLayout->setContentsMargins(16, 12, 16, 12);
    detailLayout->setSpacing(8);

    QHBoxLayout *detailHeader = new QHBoxLayout();
    m_backBtn = new QPushButton(tr("← 返回"), this);
    m_backBtn->setStyleSheet("QPushButton { background:transparent; border:none; color:palette(placeholderText); font-size:13px; } QPushButton:hover { color:palette(highlight); }");
    detailHeader->addWidget(m_backBtn);
    detailHeader->addStretch();
    m_deleteBtn = new QPushButton(tr("删除"), this);
    m_deleteBtn->setStyleSheet("QPushButton { background:transparent; border:none; color:#E64545; font-size:12px; } QPushButton:hover { color:#CC3333; }");
    detailHeader->addWidget(m_deleteBtn);
    detailLayout->addLayout(detailHeader);

    m_titleLabel = new DLabel(this);
    m_titleLabel->setStyleSheet("font-size: 16px; font-weight: 600;");
    detailLayout->addWidget(m_titleLabel);

    m_dateLabel = new DLabel(this);
    m_dateLabel->setStyleSheet("font-size: 11px; color: palette(placeholderText);");
    detailLayout->addWidget(m_dateLabel);

    // 音频播放器
    QWidget *playerWidget = new QWidget(this);
    playerWidget->setStyleSheet("background: palette(light); border-radius: 6px;");
    QHBoxLayout *playerLayout = new QHBoxLayout(playerWidget);
    playerLayout->setContentsMargins(12, 4, 12, 4);
    m_playBtn = new QPushButton(tr("▶ 播放"), this);
    m_playBtn->setStyleSheet("QPushButton { background:palette(highlight); color:palette(highlightedText); border:none; border-radius:6px; padding:4px 16px; font-size:12px; }");
    m_positionLabel = new QLabel(tr("00:00 / 00:00"), this);
    m_positionLabel->setStyleSheet("font-size:11px; color:palette(placeholderText);");
    playerLayout->addWidget(m_playBtn);
    playerLayout->addWidget(m_positionLabel, 1);
    detailLayout->addWidget(playerWidget);

    // 操作按钮
    QHBoxLayout *actionRow = new QHBoxLayout();
    actionRow->setSpacing(8);
    m_transcribeBtn = new QPushButton(tr("🎤 语音转写"), this);
    stylePrimaryBtn(m_transcribeBtn);
    m_aiSummaryBtn = new QPushButton(tr("🤖 AI 生成纪要"), this);
    stylePrimaryBtn(m_aiSummaryBtn);
    m_transcribeBtn->setStyleSheet(m_transcribeBtn->styleSheet() + "QPushButton { background:palette(mid); }");
    actionRow->addWidget(m_transcribeBtn);
    actionRow->addWidget(m_aiSummaryBtn);
    actionRow->addStretch();
    detailLayout->addLayout(actionRow);

    // 转写内容
    m_transcriptEdit = new QTextEdit(this);
    m_transcriptEdit->setPlaceholderText(tr("点击「语音转写」识别录音内容..."));
    m_transcriptEdit->setReadOnly(true);
    m_transcriptEdit->setMinimumHeight(160);
    detailLayout->addWidget(m_transcriptEdit, 3);

    // AI 摘要
    DLabel *summaryTitle = new DLabel(tr("AI 摘要"), this);
    summaryTitle->setStyleSheet("font-size:12px; font-weight:600; margin-top:4px;");
    detailLayout->addWidget(summaryTitle);
    m_summaryEdit = new QTextEdit(this);
    m_summaryEdit->setPlaceholderText(tr("AI 生成的会议纪要..."));
    m_summaryEdit->setReadOnly(true);
    m_summaryEdit->setMinimumHeight(120);
    detailLayout->addWidget(m_summaryEdit, 2);

    m_stack->addWidget(m_detailPage);

    setLayout(mainLayout);
    m_stack->setCurrentWidget(m_emptyPage);
}

void MeetingWidget::initConnections()
{
    connect(m_newBtn, &QPushButton::clicked, this, &MeetingWidget::onNewMeeting);
    connect(m_backBtn, &QPushButton::clicked, this, &MeetingWidget::showMeetingList);
    connect(m_deleteBtn, &QPushButton::clicked, this, &MeetingWidget::onDeleteMeeting);
    connect(m_playBtn, &QPushButton::clicked, this, &MeetingWidget::onPlayPause);
    connect(m_transcribeBtn, &QPushButton::clicked, this, &MeetingWidget::onTranscribe);
    connect(m_aiSummaryBtn, &QPushButton::clicked, this, &MeetingWidget::onAiSummary);
    connect(m_meetingList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        int meetingId = item->data(Qt::UserRole).toInt();
        showMeetingDetail(meetingId);
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MeetingWidget::onSearch);
    connect(m_player, &AudioPlayer::positionChanged, this, [this](qint64 pos) {
        if (m_player->durationMs() > 0)
            m_positionLabel->setText(QString("%1/%2")
                .arg(formatTime(pos)).arg(formatTime(m_player->durationMs())));
    });
    connect(m_player, &AudioPlayer::playbackStarted, this, [this]() {
        m_playBtn->setText(tr("⏸ 暂停"));
    });
    connect(m_player, &AudioPlayer::playbackPaused, this, [this]() {
        m_playBtn->setText(tr("▶ 播放"));
    });
    connect(m_player, &AudioPlayer::playbackStopped, this, [this]() {
        m_playBtn->setText(tr("▶ 播放"));
    });
    connect(m_player, &AudioPlayer::playbackFinished, this, [this]() {
        m_playBtn->setText(tr("▶ 播放"));
    });
}

void MeetingWidget::showMeetingList()
{
    m_stack->setCurrentWidget(m_listPage);
    refresh();
}

void MeetingWidget::showMeetingDetail(int meetingId)
{
    m_currentMeetingId = meetingId;
    auto *app = ShorthandApplication::instance();
    MeetingData meeting = app->meetingManager()->getMeeting(meetingId);
    m_titleLabel->setText(meeting.title.isEmpty() ? tr("无标题") : meeting.title);
    m_dateLabel->setText(QDateTime::fromSecsSinceEpoch(meeting.createdAt).toString("yyyy-MM-dd hh:mm"));

    // 加载转写内容
    QString transcriptText;
    QList<TranscriptData> transcripts = app->meetingManager()->getTranscripts(meetingId);
    for (const auto &t : transcripts) {
        transcriptText += QString("[%1] %2\n").arg(t.formattedTimestamp(), t.text);
    }
    m_transcriptEdit->setPlainText(transcriptText.trimmed());

    // 加载 AI 摘要
    m_summaryEdit->setPlainText(meeting.aiSummary);

    // 重置播放器状态
    m_playBtn->setText(tr("▶ 播放"));
    if (m_player->isPlaying()) m_player->stop();
    m_positionLabel->setText(QString("00:00 / %1").arg(formatTime(meeting.durationSecs * 1000LL)));

    m_stack->setCurrentWidget(m_detailPage);
}

void MeetingWidget::onNewMeeting()
{
    auto *app = ShorthandApplication::instance();
    MeetingData meeting;
    meeting.title = tr("会议 %1").arg(QDateTime::currentDateTime().toString("MM-dd hh:mm"));
    meeting.createdAt = QDateTime::currentSecsSinceEpoch();
    int id = app->meetingManager()->createMeeting(meeting);
    if (id > 0) {
        showMeetingDetail(id);
    }
}

void MeetingWidget::refresh()
{
    auto *app = ShorthandApplication::instance();
    QList<MeetingData> meetings = app->meetingManager()->getAllMeetings();
    populateMeetingList(meetings);
}

void MeetingWidget::populateMeetingList(const QList<MeetingData> &meetings)
{
    m_meetingList->clear();
    if (meetings.isEmpty()) {
        m_stack->setCurrentWidget(m_emptyPage);
        return;
    }
    m_stack->setCurrentWidget(m_listPage);
    for (const auto &meeting : meetings) {
        QString displayText = meeting.title.isEmpty() ? tr("无标题") : meeting.title;
        QString dateStr = QDateTime::fromSecsSinceEpoch(meeting.createdAt).toString("MM-dd hh:mm");
        QString durationStr = meeting.durationSecs > 0 ? tr(" · %1").arg(meeting.formattedDuration()) : "";

        QWidget *card = new QWidget(this);
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 8, 12, 8);
        cardLayout->setSpacing(3);

        DLabel *titleLabel = new DLabel(displayText, this);
        titleLabel->setStyleSheet("font-size: 14px; font-weight: 600;");
        titleLabel->setWordWrap(true);
        cardLayout->addWidget(titleLabel);

        DLabel *subLabel = new DLabel(dateStr + durationStr, this);
        subLabel->setStyleSheet("font-size: 11px; color: palette(placeholderText);");
        cardLayout->addWidget(subLabel);

        QListWidgetItem *item = new QListWidgetItem(m_meetingList);
        item->setData(Qt::UserRole, meeting.id);
        item->setSizeHint(QSize(0, 64));
        m_meetingList->addItem(item);
        m_meetingList->setItemWidget(item, card);
    }
}

void MeetingWidget::onPlayPause()
{
    if (m_currentMeetingId <= 0) return;
    auto *app = ShorthandApplication::instance();
    MeetingData meeting = app->meetingManager()->getMeeting(m_currentMeetingId);
    if (meeting.audioFilePath.isEmpty() || !QFile::exists(meeting.audioFilePath)) return;
    if (!m_player->isPlaying()) {
        m_player->load(meeting.audioFilePath);
        m_player->play();
        m_playBtn->setText(tr("⏸ 暂停"));
    } else {
        m_player->stop();
        m_playBtn->setText(tr("▶ 播放"));
    }
}

void MeetingWidget::onDeleteMeeting()
{
    DDialog dialog(this);
    dialog.setTitle(tr("确认删除"));
    dialog.setMessage(tr("确定要删除此会议记录吗？"));
    dialog.addButton(tr("取消"));
    dialog.addButton(tr("删除"), true, DDialog::ButtonWarning);
    if (dialog.exec() == 1) {
        auto *app = ShorthandApplication::instance();
        app->meetingManager()->deleteMeeting(m_currentMeetingId);
        m_currentMeetingId = -1;
        refresh();
    }
}

void MeetingWidget::onAiSummary()
{
    if (m_currentMeetingId <= 0) return;
    auto *app = ShorthandApplication::instance();
    auto *mgr = app->meetingManager();
    auto *ai = app->aiService();
    if (!ai || !ai->currentService()) {
        DDialog d(this);
        d.setTitle(tr("AI 未配置"));
        d.setMessage(tr("请在设置中配置 AI 服务"));
        d.addButton(tr("确定"));
        d.exec();
        return;
    }
    QList<TranscriptData> transcripts = mgr->getTranscripts(m_currentMeetingId);
    if (transcripts.isEmpty()) {
        DDialog d(this);
        d.setTitle(tr("提示"));
        d.setMessage(tr("暂无转写内容"));
        d.addButton(tr("确定"));
        d.exec();
        return;
    }
    MeetingData meeting = mgr->getMeeting(m_currentMeetingId);
    QString transcriptText;
    for (const auto &t : transcripts) {
        transcriptText += QString("[%1] %2\n").arg(t.formattedTimestamp(), t.text);
    }
    QString prompt = QString("你是一个会议纪要助手。请根据以下转写内容生成结构化会议纪要。\n会议主题：%1\n\n%2").arg(meeting.title, transcriptText);
    m_aiSummaryBtn->setEnabled(false);
    m_aiSummaryBtn->setText(tr("生成中..."));
    AiCompletionRequest req;
    AiMessage msg;
    msg.role = "user";
    msg.content = prompt;
    req.messages.append(msg);
    req.temperature = 0.5;
    req.maxTokens = 2048;
    ai->complete(req, [this, mgr, meeting](const AiCompletionResult &result) {
        m_aiSummaryBtn->setEnabled(true);
        m_aiSummaryBtn->setText(tr("AI 生成纪要"));
        if (result.success) {
            m_summaryEdit->setPlainText(result.content);
            MeetingData updated = meeting;
            updated.aiSummary = result.content;
            mgr->updateMeeting(updated);
        } else {
            DDialog d(this);
            d.setTitle(tr("AI 生成失败"));
            d.setMessage(result.errorMessage);
            d.addButton(tr("确定"));
            d.exec();
        }
    });
}

void MeetingWidget::onTranscribe()
{
    if (m_currentMeetingId <= 0) return;
    auto *app = ShorthandApplication::instance();
    auto *mgr = app->meetingManager();
    auto *asr = app->asrService();
    MeetingData meeting = mgr->getMeeting(m_currentMeetingId);
    if (meeting.audioFilePath.isEmpty() || !QFile::exists(meeting.audioFilePath)) {
        DDialog d(this);
        d.setTitle(tr("提示"));
        d.setMessage(tr("暂无录音文件，请先录音后再转写"));
        d.addButton(tr("确定"));
        d.exec();
        return;
    }
    m_transcribeBtn->setEnabled(false);
    m_transcribeBtn->setText(tr("转写中..."));
    asr->transcribe(meeting.audioFilePath, [this, mgr, meeting](const AsrResult &result) {
        m_transcribeBtn->setEnabled(true);
        m_transcribeBtn->setText(tr("🎤 语音转写"));
        if (result.success && !result.text.isEmpty()) {
            mgr->deleteTranscripts(meeting.id);

            int seq = 1;
            if (!result.segments.isEmpty()) {
                for (const auto &seg : result.segments) {
                    TranscriptData td;
                    td.meetingId = meeting.id;
                    td.sequence = seq++;
                    td.text = seg.text;
                    td.speaker = seg.speaker;
                    td.timestampMs = seg.startMs;
                    mgr->addTranscript(td);
                }
            } else {
                TranscriptData td;
                td.meetingId = meeting.id;
                td.sequence = 1;
                td.text = result.text;
                td.timestampMs = 0;
                mgr->addTranscript(td);
            }

            MeetingData updated = meeting;
            updated.status = "completed";
            mgr->updateMeeting(updated);
            showMeetingDetail(m_currentMeetingId);
            DDialog d(this);
            d.setTitle(tr("转写完成"));
            d.setMessage(tr("识别结果：%1").arg(result.text));
            d.addButton(tr("确定"));
            d.exec();
        } else {
            DDialog d(this);
            d.setTitle(tr("转写结果"));
            d.setMessage(result.text.isEmpty() ? tr("未识别到语音内容") : result.errorMessage);
            d.addButton(tr("确定"));
            d.exec();
        }
    });
}

void MeetingWidget::onSearch(const QString &keyword)
{
    if (keyword.isEmpty()) { showMeetingList(); return; }
    auto *app = ShorthandApplication::instance();
    QList<MeetingData> meetings = app->meetingManager()->searchMeetings(keyword);
    populateMeetingList(meetings);
}

QString MeetingWidget::formatTime(qint64 ms) const
{
    int secs = ms / 1000;
    int mins = secs / 60;
    secs = secs % 60;
    return QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
}
