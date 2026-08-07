// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "meetingwidget.h"
#include "application/shorthandapplication.h"
#include "core/meetingmanager.h"
#include "core/todomanager.h"
#include "services/aiservice.h"
#include "services/asrservice.h"
#include "services/exportservice.h"
#include "audio/audioplayer.h"
#include "audio/audiorecorder.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QFile>
#include <QInputDialog>
#include <QScrollBar>
#include <QTextBlock>
#include <QDir>
#include <QCheckBox>
#include <QStandardPaths>
#include <QFileInfo>
#include <QFileDialog>
#include <QMenu>
#include <QAction>
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
            border-radius: 8px; padding: 4px 24px; font-size: 13px; font-weight: 600;
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
    m_recorder = new AudioRecorder(this);
    m_recordingAnimTimer = new QTimer(this);
    m_recordingAnimTimer->setInterval(500);
    m_recordingStartTime = 0;
    connect(m_recordingAnimTimer, &QTimer::timeout, this, [this]() {
        if (m_recordingStartTime > 0) {
            int elapsed = (QDateTime::currentMSecsSinceEpoch() - m_recordingStartTime) / 1000;
            int min = elapsed / 60;
            int sec = elapsed % 60;
            m_recordingBtn->setText(tr("⏹ 停止  %1:%2").arg(min).arg(sec, 2, 10, QChar('0')));
        }
    });
    initUI();
    initConnections();
    refresh();
}

void MeetingWidget::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    mainLayout->addWidget(m_stack);

    // ─── 空状态页 ─────────────────────────────────────────────
    m_emptyPage = new QWidget(this);
    QVBoxLayout *emptyLayout = new QVBoxLayout(m_emptyPage);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(16);

    QLabel *emptyIcon = new QLabel(tr("🎤"), m_emptyPage);
    emptyIcon->setStyleSheet("font-size: 64px;");
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyIcon);

    QLabel *emptyTitle = new QLabel(tr("会议记录"), m_emptyPage);
    emptyTitle->setStyleSheet("font-size: 18px; font-weight: 700;");
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);

    QLabel *emptyDesc = new QLabel(tr("记录会议音频，支持语音转文字和 AI 摘要"), m_emptyPage);
    emptyDesc->setStyleSheet("font-size: 13px; color: palette(placeholderText);");
    emptyDesc->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyDesc);

    QPushButton *startBtn = new QPushButton(tr("🎤 开始会议"), m_emptyPage);
    startBtn->setFixedHeight(38);
    startBtn->setCursor(Qt::PointingHandCursor);
    startBtn->setStyleSheet("QPushButton { background: palette(highlight); color: palette(highlightedText); border: none; border-radius: 8px; padding: 4px 32px; font-size: 14px; font-weight: 600; } QPushButton:hover { background: palette(dark); }");
    emptyLayout->addWidget(startBtn, 0, Qt::AlignCenter);
    connect(startBtn, &QPushButton::clicked, this, &MeetingWidget::onStartRecording);

    QPushButton *manualBtn = new QPushButton(tr("📝 新建会议"), m_emptyPage);
    manualBtn->setFixedHeight(38);
    manualBtn->setCursor(Qt::PointingHandCursor);
    manualBtn->setStyleSheet("QPushButton { background: transparent; color: palette(highlight); border: 1px solid palette(highlight); border-radius: 8px; padding: 4px 32px; font-size: 14px; } QPushButton:hover { background: palette(midlight); }");
    emptyLayout->addWidget(manualBtn, 0, Qt::AlignCenter);
    connect(manualBtn, &QPushButton::clicked, this, &MeetingWidget::onNewMeeting);

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
    // 多选模式切换按钮
    m_selectModeBtn = new QPushButton(tr("☐ 多选"), this);
    m_selectModeBtn->setCheckable(true);
    m_selectModeBtn->setFixedHeight(28);
    m_selectModeBtn->setStyleSheet(
        "QPushButton { border: 1px solid palette(mid); border-radius: 4px;"
        " padding: 2px 8px; font-size: 11px; background: palette(base); }"
        "QPushButton:hover { background: palette(light); }"
        "QPushButton:checked { background: palette(highlight); color: white; border-color: palette(highlight); }");
    headerRow->addWidget(m_selectModeBtn);
    m_newBtn = new QPushButton(tr("＋ 新建"), this);
    stylePrimaryBtn(m_newBtn);
    m_newBtn->setFixedHeight(30);
    headerRow->addWidget(m_newBtn);
    listLayout->addLayout(headerRow);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("搜索会议..."));
    m_searchEdit->setFixedHeight(32);
    m_searchEdit->setStyleSheet("QLineEdit { border:1px solid palette(mid); border-radius:8px; padding:4px 12px; font-size:13px; background:palette(window); } QLineEdit:focus { border-color:palette(highlight); }");
    listLayout->addWidget(m_searchEdit);

    m_meetingList = new QListWidget(this);
    m_meetingList->setFrameShape(QFrame::NoFrame);
    m_meetingList->setSpacing(4);
    m_meetingList->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; }
        QListWidget::item {
            border-radius: 10px; padding: 12px 16px; margin: 2px 0;
            background: palette(alternateBase); border: 1px solid transparent;
        }
        QListWidget::item:hover { background: palette(midlight); border-color: palette(mid); }
        QListWidget::item:selected { background: palette(highlight); border-color: palette(highlight); }
    )");
    listLayout->addWidget(m_meetingList, 1);
    
    // ─── 批量操作工具栏 ───────────────────────────
    m_batchToolbar = new QWidget(this);
    m_batchToolbar->setStyleSheet("background: palette(midlight); border-radius: 8px; padding: 4px;");
    m_batchToolbar->hide();
    QHBoxLayout *batchLayout = new QHBoxLayout(m_batchToolbar);
    batchLayout->setContentsMargins(8, 4, 8, 4);
    batchLayout->setSpacing(6);

    m_selectionCountLabel = new DLabel(tr("已选择 0 项"), this);
    m_selectionCountLabel->setStyleSheet("font-size: 11px; color: palette(windowText);");
    batchLayout->addWidget(m_selectionCountLabel);

    batchLayout->addStretch();

    m_selectAllBtn = new QPushButton(tr("全选"), this);
    m_selectAllBtn->setFixedHeight(28);
    m_selectAllBtn->setStyleSheet(
        "QPushButton { border: 1px solid palette(mid); border-radius: 4px;"
        " padding: 2px 10px; font-size: 11px; background: palette(base); }"
        "QPushButton:hover { background: palette(highlight); color: white; }");
    batchLayout->addWidget(m_selectAllBtn);

    m_batchDeleteBtn = new QPushButton(tr("🗑 删除"), this);
    m_batchDeleteBtn->setFixedHeight(28);
    m_batchDeleteBtn->setStyleSheet(
        "QPushButton { border: 1px solid palette(mid); border-radius: 4px;"
        " padding: 2px 10px; font-size: 11px; background: palette(base); }"
        "QPushButton:hover { background: #E64545; color: white; }");
    batchLayout->addWidget(m_batchDeleteBtn);

    listLayout->addWidget(m_batchToolbar);
    m_stack->addWidget(m_listPage);

    // ─── 详情页（简洁版）────────────────────────────────────
    m_detailPage = new QWidget(this);
    QVBoxLayout *detailLayout = new QVBoxLayout(m_detailPage);
    detailLayout->setContentsMargins(16, 12, 16, 12);
    detailLayout->setSpacing(8);

    // 返回 + 标题
    QHBoxLayout *detailHeader = new QHBoxLayout();
    m_backBtn = new QPushButton(tr("← 返回列表"), this);
    m_backBtn->setStyleSheet("QPushButton { background:transparent; border:none; color:palette(placeholderText); font-size:12px; } QPushButton:hover { color:palette(highlight); }");
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

    QHBoxLayout *fileRow = new QHBoxLayout();
    m_fileLabel = new QLabel(this);
    m_fileLabel->setStyleSheet("font-size: 11px; color: palette(placeholderText);");
    m_fileLabel->setVisible(false);
    fileRow->addWidget(m_fileLabel, 1);
    m_exportBtn = new QPushButton(tr("📤 导出录音"), this);
    m_exportBtn->setFixedHeight(26);
    m_exportBtn->setStyleSheet("QPushButton { background:transparent; color:palette(highlight); border:1px solid palette(highlight); border-radius:4px; padding:2px 10px; font-size:11px; } QPushButton:hover { background:palette(midlight); }");
    m_exportBtn->setVisible(false);
    fileRow->addWidget(m_exportBtn);
    detailLayout->addLayout(fileRow);

    // 操作按钮行
    QHBoxLayout *actionRow = new QHBoxLayout();
    actionRow->setSpacing(6);
    m_playBtn = new QPushButton(tr("▶ 播放录音"), this);
    m_playBtn->setFixedHeight(32);
    m_playBtn->setVisible(false);
    m_playBtn->setStyleSheet("QPushButton { background:transparent; color:palette(highlight); border:1px solid palette(highlight); border-radius:6px; padding:4px 12px; font-size:12px; } QPushButton:hover { background:palette(midlight); }");
    m_recordingBtn = new QPushButton(tr("🎙 开始录音"), this);
    m_recordingBtn->setFixedHeight(32);
    m_recordingBtn->setStyleSheet("QPushButton { background:#E64545; color:white; border:none; border-radius:6px; padding:4px 12px; font-size:12px; } QPushButton:hover { background:#CF3A3A; }");
    m_transcribeBtn = new QPushButton(tr("🎤 语音转写"), this);
    m_transcribeBtn->setFixedHeight(32);
    m_transcribeBtn->setStyleSheet("QPushButton { background:palette(highlight); color:palette(highlightedText); border:none; border-radius:6px; padding:4px 12px; font-size:12px; } QPushButton:hover { background:palette(dark); } QPushButton:disabled { background:palette(mid); color:palette(windowText); }");
    m_aiSummaryBtn = new QPushButton(tr("🤖 AI 纪要"), this);
    m_aiSummaryBtn->setFixedHeight(32);
    m_aiSummaryBtn->setStyleSheet("QPushButton { background:transparent; color:palette(highlight); border:1px solid palette(highlight); border-radius:6px; padding:4px 12px; font-size:12px; } QPushButton:hover { background:palette(midlight); }");

    QPushButton *genTodoBtn = new QPushButton(tr("📋 生成待办"), this);
    genTodoBtn->setFixedHeight(32);
    genTodoBtn->setStyleSheet("QPushButton { background:transparent; color:#52C41A; border:1px solid #52C41A; border-radius:6px; padding:4px 12px; font-size:12px; } QPushButton:hover { background:palette(light); }");

    actionRow->addWidget(m_playBtn);
    actionRow->addWidget(m_recordingBtn);
    actionRow->addWidget(m_transcribeBtn);
    actionRow->addWidget(m_aiSummaryBtn);
    actionRow->addWidget(genTodoBtn);
    actionRow->addStretch();
    detailLayout->addLayout(actionRow);

    connect(genTodoBtn, &QPushButton::clicked, this, [this, genTodoBtn]() {
        if (m_currentMeetingId <= 0) return;
        auto *app = ShorthandApplication::instance();
        auto *mgr = app->meetingManager();
        auto *ai = app->aiService();
        if (!ai || !ai->currentService()) {
            DDialog d(this); d.setTitle(tr("AI 未配置")); d.setMessage(tr("请在设置中配置 AI 服务")); d.addButton(tr("确定")); d.exec();
            return;
        }
        QString transcript;
        QList<TranscriptData> segs = mgr->getTranscripts(m_currentMeetingId);
        for (const auto &s : segs) {
            if (!s.text.trimmed().isEmpty()) transcript += s.text + "\n";
        }
        if (transcript.trimmed().isEmpty()) {
            DDialog d(this); d.setTitle(tr("提示")); d.setMessage(tr("暂无转写内容，请先语音转写")); d.addButton(tr("确定")); d.exec();
            return;
        }
        genTodoBtn->setEnabled(false);
        genTodoBtn->setText(tr("生成中..."));

        ai->extractTodos(transcript, [this, genTodoBtn](const QList<QPair<QString, int>> &todos) {
            genTodoBtn->setEnabled(true);
            genTodoBtn->setText(tr("📋 生成待办"));
            if (todos.isEmpty()) {
                DDialog d(this); d.setTitle(tr("提取结果")); d.setMessage(tr("未从会议内容中识别出待办事项")); d.addButton(tr("确定")); d.exec();
                return;
            }
            auto *app = ShorthandApplication::instance();
            int count = 0;
            for (const auto &todo : todos) {
                TodoData td; td.title = todo.first; td.priority = todo.second;
                td.creationDatetime = QDateTime::currentSecsSinceEpoch();
                td.modificationDatetime = td.creationDatetime;
                if (app->todoManager()->createTodo(td) > 0) count++;
            }
            DDialog d(this); d.setTitle(tr("生成完成"));
            d.setMessage(tr("成功从会议中提取 %1 条待办事项").arg(count));
            d.addButton(tr("确定"));
            d.exec();
        });
    });

    // 内容区（转写 + AI 纪要）
    // 顶部说明文字，明确两个栏目的用途
    DLabel *contentDesc = new DLabel(tr("左侧「🎤 转写内容」为语音转写文字；右侧「🤖 AI 会议纪要」为 AI 自动生成的会议总结"), this);
    contentDesc->setStyleSheet("font-size: 11px; color: palette(placeholderText); padding: 2px 0 4px 0;");
    contentDesc->setWordWrap(true);
    detailLayout->addWidget(contentDesc);

    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(12);

    // 左侧：转写内容
    QWidget *transcriptPanel = new QWidget(this);
    QVBoxLayout *transcriptPanelLayout = new QVBoxLayout(transcriptPanel);
    transcriptPanelLayout->setContentsMargins(0, 0, 0, 0);
    transcriptPanelLayout->setSpacing(4);

    DLabel *transcriptTitle = new DLabel(tr("🎤 转写内容"), this);
    transcriptTitle->setStyleSheet("font-size: 12px; font-weight: 600; padding: 4px 0;");
    transcriptPanelLayout->addWidget(transcriptTitle);

    m_transcriptEdit = new QTextBrowser(this);
    m_transcriptEdit->setPlaceholderText(tr("点击「语音转写」识别录音内容..."));
    m_transcriptEdit->setReadOnly(true);
    m_transcriptEdit->setStyleSheet("QTextBrowser { border: 1px solid palette(mid); border-radius: 8px; padding: 8px; font-size: 12px; background: palette(base); }");
    m_transcriptEdit->setMinimumHeight(200);
    m_transcriptEdit->setOpenExternalLinks(false);
    m_transcriptEdit->setOpenLinks(false);
    transcriptPanelLayout->addWidget(m_transcriptEdit, 1);
    contentLayout->addWidget(transcriptPanel, 1);

    // 右侧：AI 会议纪要
    QWidget *summaryPanel = new QWidget(this);
    QVBoxLayout *summaryPanelLayout = new QVBoxLayout(summaryPanel);
    summaryPanelLayout->setContentsMargins(0, 0, 0, 0);
    summaryPanelLayout->setSpacing(4);

    DLabel *summaryTitle = new DLabel(tr("🤖 AI 会议纪要"), this);
    summaryTitle->setStyleSheet("font-size: 12px; font-weight: 600; padding: 4px 0;");
    summaryPanelLayout->addWidget(summaryTitle);

    m_summaryEdit = new QTextEdit(this);
    m_summaryEdit->setPlaceholderText(tr("AI 生成的会议纪要..."));
    m_summaryEdit->setReadOnly(true);
    m_summaryEdit->setStyleSheet("QTextEdit { border: 1px solid palette(mid); border-radius: 8px; padding: 8px; font-size: 12px; background: palette(base); }");
    m_summaryEdit->setMinimumHeight(100);
    summaryPanelLayout->addWidget(m_summaryEdit);
    contentLayout->addWidget(summaryPanel, 1);

    detailLayout->addLayout(contentLayout, 1);

    m_stack->addWidget(m_detailPage);

    setLayout(mainLayout);
    m_stack->setCurrentWidget(m_emptyPage);
}

void MeetingWidget::initConnections()
{
    connect(m_newBtn, &QPushButton::clicked, this, &MeetingWidget::onNewMeeting);
    connect(m_backBtn, &QPushButton::clicked, this, &MeetingWidget::showMeetingList);
    connect(m_deleteBtn, &QPushButton::clicked, this, &MeetingWidget::onDeleteMeeting);
    connect(m_transcribeBtn, &QPushButton::clicked, this, &MeetingWidget::onTranscribe);
    connect(m_aiSummaryBtn, &QPushButton::clicked, this, &MeetingWidget::onAiSummary);
    connect(m_playBtn, &QPushButton::clicked, this, &MeetingWidget::onTogglePlayback);
    connect(m_player, &AudioPlayer::playbackFinished, this, [this]() {
        m_playBtn->setText(tr("▶ 播放录音"));
    });
    connect(m_player, &AudioPlayer::playbackStopped, this, [this]() {
        m_playBtn->setText(tr("▶ 播放录音"));
    });
    connect(m_recordingBtn, &QPushButton::clicked, this, &MeetingWidget::onStartRecording);
    connect(m_recorder, &AudioRecorder::recordingFinished, this, &MeetingWidget::onStopRecording);
    
    // 多选模式切换
    connect(m_selectModeBtn, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) enterMultiSelectMode();
        else exitMultiSelectMode();
    });
    
    // 全选按钮
    connect(m_selectAllBtn, &QPushButton::clicked, this, [this]() {
        bool allSelected = true;
        for (int i = 0; i < m_meetingList->count(); ++i) {
            QListWidgetItem *item = m_meetingList->item(i);
            QWidget *w = m_meetingList->itemWidget(item);
            if (w) {
                QCheckBox *cb = w->findChild<QCheckBox *>("selectCheck");
                if (cb && !cb->isChecked()) {
                    allSelected = false;
                    break;
                }
            }
        }
        bool check = !allSelected;
        for (int i = 0; i < m_meetingList->count(); ++i) {
            QListWidgetItem *item = m_meetingList->item(i);
            QWidget *w = m_meetingList->itemWidget(item);
            if (w) {
                QCheckBox *cb = w->findChild<QCheckBox *>("selectCheck");
                if (cb) cb->setChecked(check);
            }
        }
        updateSelectionState();
    });
    
    // 批量删除
    connect(m_batchDeleteBtn, &QPushButton::clicked, this, &MeetingWidget::onBatchDelete);
    
    // 多选模式下点击列表项切换选择状态
    connect(m_meetingList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        int meetingId = item->data(Qt::UserRole).toInt();
        if (m_multiSelectMode) {
            QWidget *w = m_meetingList->itemWidget(item);
            if (w) {
                QCheckBox *cb = w->findChild<QCheckBox *>("selectCheck");
                if (cb) {
                    cb->setChecked(!cb->isChecked());
                    updateSelectionState();
                }
            }
        } else if (meetingId > 0) {
            showMeetingDetail(meetingId);
        }
    });

    // 右键菜单
    m_meetingList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_meetingList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *item = m_meetingList->itemAt(pos);
        if (!item) return;
        int meetingId = item->data(Qt::UserRole).toInt();
        if (meetingId <= 0) return;

        QMenu menu(this);
        // 跟随系统主题（深色/浅色均可读），选中项使用主题高亮色
        menu.setStyleSheet(R"(
            QMenu { background: palette(window); border: 1px solid palette(mid); border-radius: 6px; padding: 4px; }
            QMenu::item { padding: 6px 24px; border-radius: 4px; font-size: 13px; color: palette(windowText); }
            QMenu::item:selected { background: palette(highlight); color: palette(highlightedText); }
            QMenu::separator { height: 1px; background: palette(midlight); margin: 4px 8px; }
        )");

        QAction *openAction = menu.addAction(tr("📂 打开"));
        connect(openAction, &QAction::triggered, this, [this, meetingId]() { showMeetingDetail(meetingId); });

        menu.addSeparator();

        QAction *transcribeAction = menu.addAction(tr("🎤 语音转写"));
        connect(transcribeAction, &QAction::triggered, this, [this, meetingId]() {
            m_currentMeetingId = meetingId;
            onTranscribe();
        });

        QAction *summaryAction = menu.addAction(tr("🤖 AI 生成纪要"));
        connect(summaryAction, &QAction::triggered, this, [this, meetingId]() {
            m_currentMeetingId = meetingId;
            onAiSummary();
        });

        menu.addSeparator();

        QAction *deleteAction = menu.addAction(tr("🗑 删除"));
        connect(deleteAction, &QAction::triggered, this, [this, meetingId]() {
            m_currentMeetingId = meetingId;
            onDeleteMeeting();
        });

        menu.exec(m_meetingList->viewport()->mapToGlobal(pos));
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MeetingWidget::onSearch);
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

    // 显示录音文件信息
    if (!meeting.audioFilePath.isEmpty() && QFile::exists(meeting.audioFilePath)) {
        QFileInfo fi(meeting.audioFilePath);
        m_fileLabel->setText(tr("录音文件: %1").arg(fi.fileName()));
        m_fileLabel->setVisible(true);
        m_exportBtn->setVisible(true);
        m_playBtn->setVisible(true);
        m_playBtn->setText(m_player->isPlaying() ? tr("⏹ 停止播放") : tr("▶ 播放录音"));
        // 断开旧连接避免重复
        disconnect(m_exportBtn, &QPushButton::clicked, nullptr, nullptr);
        connect(m_exportBtn, &QPushButton::clicked, this, [this, meeting]() {
            QString savePath = QFileDialog::getSaveFileName(this, tr("导出录音"),
                QFileInfo(meeting.audioFilePath).fileName(),
                tr("音频文件 (*.wav *.mp3 *.ogg)"));
            if (!savePath.isEmpty()) {
                bool ok = false;
                if (auto *app = ShorthandApplication::instance()) {
                    if (auto *es = app->exportService()) {
                        ok = es->exportMeetingAudio(meeting.audioFilePath, savePath);
                    }
                }
                DDialog d(this);
                d.setTitle(ok ? tr("导出成功") : tr("导出失败"));
                d.setMessage(ok ? tr("录音已导出到：%1").arg(savePath)
                                : tr("录音导出失败，请检查录音文件是否仍存在。"));
                d.addButton(tr("确定"));
                d.exec();
            }
        });
    } else {
        m_fileLabel->setVisible(false);
        m_exportBtn->setVisible(false);
        m_playBtn->setVisible(false);
        m_playBtn->setText(tr("▶ 播放录音"));
    }

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

    // 重置高亮并缓存音频路径
    m_highlightedSegmentIndex = -1;
    m_currentAudioFilePath = meeting.audioFilePath;

    // 如果有音频文件，预加载
    if (!m_currentAudioFilePath.isEmpty() && QFile::exists(m_currentAudioFilePath)) {
        m_player->load(m_currentAudioFilePath);
        if (!m_player->isPlaying()) {
            m_player->stop();
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

void MeetingWidget::onStartRecording()
{
    // 如果正在录音，点击则停止
    if (m_recorder->state() == AudioRecorder::Recording) {
        m_recorder->stopRecording();
        return;
    }

    // 创建会议并开始录音
    auto *app = ShorthandApplication::instance();
    MeetingData meeting;
    meeting.title = tr("会议 %1").arg(QDateTime::currentDateTime().toString("MM-dd hh:mm"));
    meeting.createdAt = QDateTime::currentSecsSinceEpoch();
    meeting.status = "recording";
    int id = app->meetingManager()->createMeeting(meeting);

    if (id > 0) {
        m_currentMeetingId = id;
        // 录音文件路径：使用设置页配置的存储目录（未配置时回退到默认目录）
        QString audioPath = AudioRecorder::makeRecordingPath("会议_");

        if (m_recorder->startRecording(audioPath)) {
            m_recordingBtn->setText(tr("⏹ 停止  0:00"));
            m_recordingBtn->setStyleSheet("QPushButton { background:#E64545; color:white; border:none; border-radius:6px; padding:4px 16px; font-size:12px; } QPushButton:hover { background:#CF3A3A; }");
            m_recordingStartTime = QDateTime::currentMSecsSinceEpoch();
            m_recordingAnimTimer->start();
            showMeetingDetail(id);
        } else {
            DDialog d(this);
            d.setTitle(tr("录音失败"));
            d.setMessage(tr("无法启动录音，请检查麦克风。"));
            d.addButton(tr("确定"));
            d.exec();
        }
    }
}

void MeetingWidget::onStopRecording(const QString &filePath)
{
    m_recordingAnimTimer->stop();
    m_recordingBtn->setText(tr("🎙 开始录音"));
    m_recordingBtn->setStyleSheet("QPushButton { background:#E64545; color:white; border:none; border-radius:6px; padding:4px 16px; font-size:12px; } QPushButton:hover { background:#CF3A3A; }");

    if (m_currentMeetingId <= 0) return;

    auto *app = ShorthandApplication::instance();
    MeetingData meeting = app->meetingManager()->getMeeting(m_currentMeetingId);
    if (meeting.id <= 0) return;

    meeting.audioFilePath = filePath;
    meeting.status = "completed";
    meeting.endedAt = QDateTime::currentSecsSinceEpoch();
    meeting.durationSecs = (int)(meeting.endedAt - meeting.createdAt);
    app->meetingManager()->updateMeeting(meeting);
    m_currentAudioFilePath = filePath;

    // 刷新详情页，展示录音文件名与导出按钮
    showMeetingDetail(m_currentMeetingId);

    DDialog d(this);
    d.setTitle(tr("录音完成"));
    d.setMessage(tr("会议录音已保存，可点击「🎤 语音转写」识别内容。"));
    d.addButton(tr("确定"));
    d.exec();
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
    // 始终停留在列表页（TC06 六轮④：保证「会议主页右侧始终有历史会议清单」入口，
    // 不切回纯空状态页导致看不到入口）
    m_stack->setCurrentWidget(m_listPage);

    if (meetings.isEmpty()) {
        // 搜索时显示「无匹配结果」；非搜索时显示「暂无历史会议」引导
        QListWidgetItem *hint = new QListWidgetItem(
            m_searching ? tr("没有匹配的会议") : tr("暂无历史会议，点击右上角「＋ 新建」或「🎤 开始录音」创建"));
        hint->setFlags(Qt::NoItemFlags);
        hint->setForeground(palette().color(QPalette::PlaceholderText));
        hint->setTextAlignment(Qt::AlignCenter);
        hint->setSizeHint(QSize(0, 80));
        m_meetingList->addItem(hint);
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
        // 带上说话人标签，便于 AI 纪要按发言人组织内容（V2-T1）
        QString speaker = t.speaker.isEmpty() ? tr("说话人") : t.speaker;
        transcriptText += QString("[%1] %2: %3\n").arg(t.formattedTimestamp(), speaker, t.text);
    }
    QString prompt = QString("你是一个会议纪要助手。请根据以下转写内容生成结构化会议纪要，并按发言人（说话人1/说话人2…）组织要点。\n会议主题：%1\n\n%2").arg(meeting.title, transcriptText);
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

void MeetingWidget::onTogglePlayback()
{
    if (m_currentAudioFilePath.isEmpty() || !QFile::exists(m_currentAudioFilePath)) {
        DDialog d(this);
        d.setTitle(tr("提示"));
        d.setMessage(tr("暂无录音文件，请先录音"));
        d.addButton(tr("确定"));
        d.exec();
        return;
    }

    if (m_player->isPlaying()) {
        m_player->stop();
        m_playBtn->setText(tr("▶ 播放录音"));
    } else {
        if (!m_player->isLoaded() || m_player->currentFile() != m_currentAudioFilePath) {
            m_player->load(m_currentAudioFilePath);
        }
        m_player->play();
        m_playBtn->setText(tr("⏹ 停止播放"));
    }
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
    if (keyword.isEmpty()) { m_searching = false; showMeetingList(); return; }
    m_searching = true;
    auto *app = ShorthandApplication::instance();
    QList<MeetingData> meetings = app->meetingManager()->searchMeetings(keyword);
    populateMeetingList(meetings);
}

// ─── 时间戳点击跳转 ──────────────────────────────────────────



// ─── 构建转写 HTML（可点击时间戳） ───────────────────────────

QString MeetingWidget::buildTranscriptHtml(int highlightIndex) const
{
    if (m_currentTranscripts.isEmpty()) return QString();

    // 从系统调色板获取实际颜色值（QTextDocument HTML 不支持 palette() 函数）
    QColor hlBg = palette().color(QPalette::Highlight);
    QColor hlText = palette().color(QPalette::HighlightedText);
    QColor linkClr = palette().color(QPalette::Link);
    QString hlBgName = hlBg.name();
    QString hlTextName = hlText.name();
    QString linkColorName = linkClr.name();

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
            speakerTag = QStringLiteral("<span style='color:%1; font-size:11px; font-weight:600;'>%2 </span>")
                             .arg(palette().color(QPalette::PlaceholderText).name(), t.speaker.toHtmlEscaped());
        }

        QString bgStyle;
        QString curLinkColor = linkColorName;
        QString textStyle;
        bool isHighlight = (i == highlightIndex);

        if (isHighlight) {
            bgStyle = QStringLiteral("background:%1; color:%2;").arg(hlBgName, hlTextName);
            curLinkColor = hlTextName;
            textStyle = QStringLiteral("color:%1;").arg(hlTextName);
        }

        // 构建单条转写：可点击时间戳 + 说话人 + 文本
        html += QStringLiteral(
            "<p id='seg_%1' style='margin:4px 0; padding:4px 8px; border-radius:4px; %2'>"
            "<a href='%3' style='color:%4; text-decoration:none; font-weight:600;'>[%5]</a> "
            "<span style='%6'>%7%8</span></p>"
        ).arg(i)
         .arg(bgStyle)
         .arg(tsLink, curLinkColor, tsStr)
         .arg(textStyle, speakerTag, t.text.toHtmlEscaped());
    }

    html += QStringLiteral("</body></html>");
    return html;
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

// ─── 多选模式 ────────────────────────────────────────────────────

void MeetingWidget::enterMultiSelectMode()
{
    m_multiSelectMode = true;
    m_batchToolbar->show();
    refresh();
    updateSelectionState();
}

void MeetingWidget::exitMultiSelectMode()
{
    m_multiSelectMode = false;
    m_batchToolbar->hide();
    refresh();
}

void MeetingWidget::updateSelectionState()
{
    QList<int> selected = getSelectedMeetingIds();
    int count = selected.size();
    m_selectionCountLabel->setText(tr("已选择 %1 项").arg(count));
    m_batchDeleteBtn->setEnabled(count > 0);
}

QList<int> MeetingWidget::getSelectedMeetingIds() const
{
    QList<int> ids;
    for (int i = 0; i < m_meetingList->count(); ++i) {
        QListWidgetItem *item = m_meetingList->item(i);
        QWidget *w = m_meetingList->itemWidget(item);
        if (w) {
            QCheckBox *cb = w->findChild<QCheckBox *>("selectCheck");
            if (cb && cb->isChecked()) {
                int meetingId = item->data(Qt::UserRole).toInt();
                if (meetingId > 0) ids.append(meetingId);
            }
        }
    }
    return ids;
}

void MeetingWidget::onBatchDelete()
{
    QList<int> ids = getSelectedMeetingIds();
    if (ids.isEmpty()) return;

    auto *app = ShorthandApplication::instance();
    if (!app || !app->meetingManager()) return;

    DDialog dialog(this);
    dialog.setTitle(tr("批量删除"));
    dialog.setMessage(tr("确定要删除选中的 %1 条会议记录吗？").arg(ids.size()));
    dialog.addButton(tr("取消"));
    dialog.addButton(tr("删除"), true, DDialog::ButtonWarning);
    if (dialog.exec() == 1) {
        app->meetingManager()->batchDeleteMeetings(ids);
        m_selectModeBtn->setChecked(false);
        m_currentMeetingId = -1;
        m_currentTranscripts.clear();
        m_highlightedSegmentIndex = -1;
        refresh();
    }
}
