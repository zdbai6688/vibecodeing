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
#include <QScrollBar>
#include <QTextBlock>
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

    // ─── 空状态页 ─────────────────────────────────────────────
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

    // ─── 列表页 ────────────────────────────────────────────────
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

    // ─── 详情页 ────────────────────────────────────────────────
    m_detailPage = new QWidget(this);
    QVBoxLayout *detailLayout = new QVBoxLayout(m_detailPage);
    detailLayout->setContentsMargins(16, 12, 16, 12);
    detailLayout->setSpacing(8);

    // 标题栏
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

    // 转写内容（QTextBrowser 支持锚点点击）
    m_transcriptEdit = new QTextBrowser(this);
    m_transcriptEdit->setPlaceholderText(tr("点击「语音转写」识别录音内容..."));
    m_transcriptEdit->setReadOnly(true);
    m_transcriptEdit->setMinimumHeight(160);
    m_transcriptEdit->setOpenExternalLinks(false);
    m_transcriptEdit->setOpenLinks(false);
    detailLayout->addWidget(m_transcriptEdit, 3);

    // AI 摘要
    DLabel *summaryTitle = new DLabel(tr("AI 摘要"), this);
    summaryTitle->setStyleSheet("font-size:12px; font-weight:600;");
    detailLayout->addWidget(summaryTitle);
    m_summaryEdit = new QTextEdit(this);
    m_summaryEdit->setPlaceholderText(tr("AI 生成的会议纪要..."));
    m_summaryEdit->setReadOnly(true);
    m_summaryEdit->setMinimumHeight(140);
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

    // 时间戳点击 → 跳转到音频对应位置
    connect(m_transcriptEdit, &QTextBrowser::anchorClicked,
            this, &MeetingWidget::onTranscriptAnchorClicked);

    // 播放位置变化 → 更新显示并高亮当前转写段
    connect(m_player, &AudioPlayer::positionChanged,
            this, &MeetingWidget::onPlaybackPositionChanged);

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
        m_highlightedSegmentIndex = -1;
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
    auto *mgr = app->meetingManager();
    MeetingData meeting = mgr->getMeeting(meetingId);

    m_titleLabel->setText(meeting.title.isEmpty() ? tr("无标题") : meeting.title);
    m_dateLabel->setText(QDateTime::fromSecsSinceEpoch(meeting.createdAt).toString("yyyy-MM-dd hh:mm"));

    // 加载转写文本（带可点击时间戳）
    m_currentTranscripts = mgr->getTranscripts(meetingId);
    if (!m_currentTranscripts.isEmpty()) {
        m_transcriptEdit->setHtml(buildTranscriptHtml());
    } else {
        m_transcriptEdit->clear();
        m_transcriptEdit->setPlaceholderText(tr("点击「语音转写」识别录音内容..."));
    }

    // 加载 AI 摘要
    m_summaryEdit->setPlainText(meeting.aiSummary);

    // 重置高亮
    m_highlightedSegmentIndex = -1;

    // 如果有音频文件，预加载
    if (!meeting.audioFilePath.isEmpty() && QFile::exists(meeting.audioFilePath)) {
        m_player->load(meeting.audioFilePath);
        if (!m_player->isPlaying()) {
            m_player->stop();
            m_positionLabel->setText(tr("00:00 / 00:00"));
        }
    }

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

    for (const auto &m : meetings) {
        QString display = m.title;
        if (m.createdAt > 0) {
            display += "\n" + QDateTime::fromSecsSinceEpoch(m.createdAt).toString("yyyy-MM-dd hh:mm");
        }
        if (!m.formattedDuration().isEmpty()) {
            display += "  ⏱ " + m.formattedDuration();
        }
        QListWidgetItem *item = new QListWidgetItem(display, m_meetingList);
        item->setData(Qt::UserRole, m.id);
        item->setToolTip(m.title);
    }
}

void MeetingWidget::onPlayPause()
{
    if (m_currentMeetingId <= 0) return;
    auto *app = ShorthandApplication::instance();
    MeetingData meeting = app->meetingManager()->getMeeting(m_currentMeetingId);
    if (meeting.audioFilePath.isEmpty() || !QFile::exists(meeting.audioFilePath)) {
        DDialog d(this);
        d.setTitle(tr("提示"));
        d.setMessage(tr("暂无录音文件"));
        d.addButton(tr("确定"));
        d.exec();
        return;
    }

    if (m_player->isPlaying()) {
        // 正在播放 → 暂停
        m_player->pause();
    } else {
        // 暂停或停止 → 开始/继续播放
        if (!m_player->isLoaded()) {
            m_player->load(meeting.audioFilePath);
        }
        m_player->play();
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
        m_currentTranscripts.clear();
        m_highlightedSegmentIndex = -1;
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

// ─── 时间戳点击跳转 ──────────────────────────────────────────

void MeetingWidget::onTranscriptAnchorClicked(const QUrl &link)
{
    // 链接格式: "seek://TIMESTAMP_MS"
    if (link.scheme() != "seek") return;

    bool ok = false;
    QString path = link.path();
    // 去掉开头的 "/"
    if (path.startsWith('/')) path = path.mid(1);
    qint64 ts = path.toLongLong(&ok);

    if (!ok || m_currentMeetingId <= 0) return;

    auto *app = ShorthandApplication::instance();
    MeetingData meeting = app->meetingManager()->getMeeting(m_currentMeetingId);
    if (meeting.audioFilePath.isEmpty() || !QFile::exists(meeting.audioFilePath)) return;

    // 跳转到对应时间位置
    if (!m_player->isLoaded()) {
        m_player->load(meeting.audioFilePath);
    }
    m_player->seekTo(ts);

    // 如果播放器不在播放状态，开始播放
    if (!m_player->isPlaying()) {
        m_player->play();
    }
}

void MeetingWidget::onPlaybackPositionChanged(qint64 posMs)
{
    // 更新位置标签
    auto *app = ShorthandApplication::instance();
    MeetingData meeting = app->meetingManager()->getMeeting(m_currentMeetingId);
    if (!meeting.audioFilePath.isEmpty() && m_player->isLoaded()) {
        m_positionLabel->setText(QString("%1 / %2")
            .arg(formatTime(posMs)).arg(formatTime(m_player->durationMs())));
    }

    // 高亮当前对应的转写段
    highlightTranscriptAtPosition(posMs);
}

// ─── 构建转写 HTML（可点击时间戳） ───────────────────────────

QString MeetingWidget::buildTranscriptHtml() const
{
    if (m_currentTranscripts.isEmpty()) return QString();

    QString html;
    html += QStringLiteral("<html><body style='font-size:13px; line-height:1.6;'>");

    for (int i = 0; i < m_currentTranscripts.size(); ++i) {
        const TranscriptData &t = m_currentTranscripts[i];

        // 时间戳作为可点击锚点
        QString tsStr = t.formattedTimestamp();
        QString tsLink = QStringLiteral("seek://%1").arg(t.timestampMs);

        // 说话人标签
        QString speakerTag;
        if (!t.speaker.isEmpty()) {
            speakerTag = QStringLiteral("<span style='color:#666; font-size:11px; font-weight:600;'>%1 </span>")
                             .arg(t.speaker.toHtmlEscaped());
        }

        // 构建单条转写：可点击时间戳 + 说话人 + 文本
        html += QStringLiteral(
            "<p id='seg_%1' style='margin:4px 0; padding:4px 8px; border-radius:4px;'>"
            "<a href='%2' style='color:palette(highlight); text-decoration:none; font-weight:600;'>[%3]</a> "
            "%4%5</p>"
        ).arg(i)
         .arg(tsLink, tsStr)
         .arg(speakerTag, t.text.toHtmlEscaped());
    }

    html += QStringLiteral("</body></html>");
    return html;
}

// ─── 高亮当前播放位置对应的转写段 ────────────────────────────

void MeetingWidget::highlightTranscriptAtPosition(qint64 posMs)
{
    if (m_currentTranscripts.isEmpty()) return;

    // 找到 posMs 所在的 segment 索引（二分查找优化）
    int newIndex = -1;
    for (int i = m_currentTranscripts.size() - 1; i >= 0; --i) {
        if (m_currentTranscripts[i].timestampMs <= posMs) {
            newIndex = i;
            break;
        }
    }

    // 如果没变化则跳过
    if (newIndex == m_highlightedSegmentIndex) return;

    m_highlightedSegmentIndex = newIndex;

    // 重新构建 HTML 时标记当前段高亮
    QString html;
    html += QStringLiteral("<html><body style='font-size:13px; line-height:1.6;'>");

    for (int i = 0; i < m_currentTranscripts.size(); ++i) {
        const TranscriptData &t = m_currentTranscripts[i];
        QString tsStr = t.formattedTimestamp();
        QString tsLink = QStringLiteral("seek://%1").arg(t.timestampMs);

        QString speakerTag;
        if (!t.speaker.isEmpty()) {
            speakerTag = QStringLiteral("<span style='color:#666; font-size:11px; font-weight:600;'>%1 </span>")
                             .arg(t.speaker.toHtmlEscaped());
        }

        QString bgStyle;
        QString linkColor = QStringLiteral("palette(highlight)");
        QString textColor;

        if (i == newIndex) {
            bgStyle = QStringLiteral("background:palette(highlight); color:palette(highlightedText);");
            linkColor = QStringLiteral("palette(highlightedText)");
            textColor = QStringLiteral("color:palette(highlightedText);");
        }

        html += QStringLiteral(
            "<p id='seg_%1' style='margin:4px 0; padding:4px 8px; border-radius:4px; %2'>"
            "<a href='%3' style='color:%4; text-decoration:none; font-weight:600;'>[%5]</a> "
            "<span style='%6'>%7%8</span></p>"
        ).arg(i)
         .arg(bgStyle)
         .arg(tsLink, linkColor, tsStr)
         .arg(textColor, speakerTag, t.text.toHtmlEscaped());
    }

    html += QStringLiteral("</body></html>");
    m_transcriptEdit->setHtml(html);

    // 滚动到高亮行
    if (newIndex >= 0) {
        QTextDocument *doc = m_transcriptEdit->document();
        QTextBlock block = doc->findBlockByNumber(newIndex);
        if (block.isValid()) {
            QTextCursor scrollCursor(block);
            m_transcriptEdit->setTextCursor(scrollCursor);
            m_transcriptEdit->ensureCursorVisible();
        }
    }
}

QString MeetingWidget::formatTime(qint64 ms) const
{
    if (ms < 0) ms = 0;
    int totalSecs = ms / 1000;
    int hours = totalSecs / 3600;
    int mins = (totalSecs % 3600) / 60;
    int secs = totalSecs % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}
