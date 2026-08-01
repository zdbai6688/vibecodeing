#include "noteeditorwidget.h"
#include "markdownhighlighter.h"
#include "application/shorthandapplication.h"
#include "core/notemanager.h"
#include "core/todomanager.h"
#include "core/tagmanager.h"
#include "storage/todostorage.h"
#include "services/aiservice.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <DLabel>
#include <DDialog>
#include <DMessageBox>
#include <QIcon>
#include <DFontSizeManager>
#include <QDebug>
#include <QFileDialog>
#include <QFile>
#include <QTextBlockFormat>
#include <QProgressDialog>
#include <QToolButton>
#include <QPair>
#include <QFontDatabase>
#include <QColorDialog>
#include <DGuiApplicationHelper>

NoteEditorWidget::NoteEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    qInfo() << "[Editor] 构造开始";
    m_screenshotMgr = new ScreenshotManager(this);
    qInfo() << "[Editor] ScreenshotManager";
    m_recorder = new AudioRecorder(this);
    qInfo() << "[Editor] AudioRecorder";
    setStyleSheet("NoteEditorWidget { background: palette(base); }");
    initUI();
    qInfo() << "[Editor] initUI";
    initConnections();
    qInfo() << "[Editor] initConnections";
}

void NoteEditorWidget::setupToolbar(QVBoxLayout *mainLayout)
{
    QWidget *toolbarWidget = new QWidget(this);
    toolbarWidget->setFixedHeight(40);
    QHBoxLayout *toolbar = new QHBoxLayout(toolbarWidget);
    toolbar->setContentsMargins(12, 0, 12, 0);
    toolbar->setSpacing(2);

    auto makeBtn = [this, toolbar](const QString &text, const QString &tip) -> QToolButton* {
        QToolButton *btn = new QToolButton(this);
        btn->setText(text);
        btn->setFixedSize(30, 28);
        btn->setToolTip(tip);
        btn->setStyleSheet(R"(
            QToolButton { background:transparent; border:none; border-radius:6px; font-size:13px; color:palette(windowText); }
            QToolButton:hover { background:palette(light); }
        )");
        toolbar->addWidget(btn);
        return btn;
    };

    m_undoBtn = makeBtn("↩", tr("撤销"));
    m_redoBtn = makeBtn("↪", tr("回退"));
    toolbar->addSpacing(8);

    m_fontCombo = new QComboBox(this);
    m_fontCombo->setFixedHeight(26);
    m_fontCombo->setFixedWidth(100);
    m_fontCombo->addItems(QFontDatabase().families(QFontDatabase::SimplifiedChinese).mid(0, 10));
    m_fontCombo->setStyleSheet("QComboBox { border:1px solid palette(mid); border-radius:6px; padding:2px 4px; font-size:11px; }");
    toolbar->addWidget(m_fontCombo);

    m_sizeCombo = new QComboBox(this);
    m_sizeCombo->setFixedHeight(26);
    m_sizeCombo->setFixedWidth(56);
    QStringList sizes = {"12", "14", "16", "18", "20", "24", "28", "36"};
    m_sizeCombo->addItems(sizes);
    m_sizeCombo->setCurrentText("14");
    m_sizeCombo->setStyleSheet("QComboBox { border:1px solid palette(mid); border-radius:6px; padding:2px 4px; font-size:11px; }");
    toolbar->addWidget(m_sizeCombo);
    toolbar->addSpacing(8);

    QToolButton *colorBtn = makeBtn("●", tr("主题色"));
    colorBtn->setStyleSheet("QToolButton { background:transparent; border:none; border-radius:6px; font-size:16px; color:palette(highlight); } QToolButton:hover { background:palette(light); }");
    connect(colorBtn, &QToolButton::clicked, this, [this]() {
        auto *helper = DGuiApplicationHelper::instance();
        QColor defaultColor = helper && helper->themeType() == DGuiApplicationHelper::DarkType
            ? QColor("#78A9FF") : QColor("#2178E5");
        QColor c = QColorDialog::getColor(defaultColor, this, tr("选择主题色"));
        if (c.isValid()) {
            m_contentEdit->setStyleSheet(m_contentEdit->styleSheet() + QString("QTextEdit { color: %1; }").arg(c.name()));
        }
    });

    m_boldBtn = makeBtn("B", tr("加粗"));
    m_boldBtn->setCheckable(true);
    m_boldBtn->setStyleSheet("QToolButton { background:transparent; border:none; border-radius:6px; font-size:13px; font-weight:bold; color:palette(windowText); } QToolButton:hover { background:palette(light); } QToolButton:checked { color:palette(highlight); }");

    m_italicBtn = makeBtn("I", tr("斜体"));
    m_italicBtn->setCheckable(true);
    m_italicBtn->setStyleSheet("QToolButton { background:transparent; border:none; border-radius:6px; font-size:13px; font-style:italic; color:palette(windowText); } QToolButton:hover { background:palette(light); } QToolButton:checked { color:palette(highlight); }");

    m_underlineBtn = makeBtn("U", tr("下划线"));
    m_underlineBtn->setCheckable(true);
    m_underlineBtn->setStyleSheet("QToolButton { background:transparent; border:none; border-radius:6px; font-size:13px; text-decoration:underline; color:palette(windowText); } QToolButton:hover { background:palette(light); } QToolButton:checked { color:palette(highlight); }");

    m_olBtn = makeBtn("1.", tr("有序列表"));
    m_ulBtn = makeBtn("•", tr("无序列表"));
    m_imageBtn = makeBtn("🖼", tr("插入图片"));
    toolbar->addStretch();

    connect(m_boldBtn, &QToolButton::clicked, this, [this]() {
        QTextCursor cursor = m_contentEdit->textCursor();
        QString selected = cursor.selectedText();
        if (selected.isEmpty()) {
            cursor.insertText("****");
            cursor.setPosition(cursor.position() - 2);
        } else {
            cursor.insertText("**" + selected + "**");
        }
        m_contentEdit->setFocus();
        m_modified = true;
    });
    connect(m_italicBtn, &QToolButton::clicked, this, [this]() {
        QTextCursor cursor = m_contentEdit->textCursor();
        QString selected = cursor.selectedText();
        if (selected.isEmpty()) {
            cursor.insertText("**");
            cursor.setPosition(cursor.position() - 1);
        } else {
            cursor.insertText("*" + selected + "*");
        }
        m_contentEdit->setFocus();
        m_modified = true;
    });
    connect(m_underlineBtn, &QToolButton::clicked, this, [this]() {
        QTextCursor cursor = m_contentEdit->textCursor();
        QString selected = cursor.selectedText();
        if (selected.isEmpty()) {
            // 下划线在Markdown中用HTML标记
            cursor.insertText("<u></u>");
            cursor.setPosition(cursor.position() - 4);
        } else {
            cursor.insertText("<u>" + selected + "</u>");
        }
        m_contentEdit->setFocus();
        m_modified = true;
    });
    connect(m_olBtn, &QToolButton::clicked, this, [this]() {
        m_contentEdit->textCursor().insertText("1. ");
        m_contentEdit->setFocus(); m_modified = true;
    });
    connect(m_ulBtn, &QToolButton::clicked, this, [this]() {
        m_contentEdit->textCursor().insertText("- ");
        m_contentEdit->setFocus(); m_modified = true;
    });
    connect(m_imageBtn, &QToolButton::clicked, this, &NoteEditorWidget::onInsertImage);

    mainLayout->addWidget(toolbarWidget);
}

void NoteEditorWidget::onInsertImage()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择图片"), QString(), tr("图片 (*.png *.jpg *.jpeg *.gif *.bmp)"));
    if (!filePath.isEmpty()) {
        m_contentEdit->textCursor().insertText(QString("![](%1)").arg(filePath));
        m_modified = true;
    }
}

void NoteEditorWidget::onUndo() { m_contentEdit->undo(); }
void NoteEditorWidget::onRedo() { m_contentEdit->redo(); }

void NoteEditorWidget::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 标题栏 + 右上角操作
    QWidget *headerBar = new QWidget(this);
    headerBar->setFixedHeight(48);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(16, 0, 12, 0);
    headerLayout->setSpacing(4);

    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setPlaceholderText(tr("无标题"));
    m_titleEdit->setStyleSheet("QLineEdit { border: none; font-size: 18px; font-weight: 600; background: transparent; padding: 0; }");
    headerLayout->addWidget(m_titleEdit, 1);

    m_todoToggleBtn = new QPushButton(tr("转为待办"), this);
    m_todoToggleBtn->setFixedHeight(26);
    m_todoToggleBtn->setStyleSheet("QPushButton { background:palette(light); color:palette(highlight); border:1px solid palette(highlight); border-radius:6px; padding:2px 10px; font-size:11px; } QPushButton:hover { background:palette(highlight); }");
    headerLayout->addWidget(m_todoToggleBtn);

    m_deleteBtn = new QPushButton(tr("删除"), this);
    m_deleteBtn->setFixedHeight(26);
    m_deleteBtn->setStyleSheet("QPushButton { background:transparent;  border:1px solid palette(mid); border-radius:6px; padding:2px 10px; font-size:11px; } QPushButton:hover {  }");
    headerLayout->addWidget(m_deleteBtn);

    mainLayout->addWidget(headerBar);

    // 工具栏
    setupToolbar(mainLayout);

    // 内容编辑区
    m_contentStack = new QStackedWidget(this);
    m_contentEdit = new QTextEdit(this);
    m_contentEdit->setPlaceholderText(tr("开始输入内容..."));
    m_contentEdit->setAcceptRichText(false);
    m_contentEdit->setFrameShape(QFrame::NoFrame);
    m_contentEdit->setStyleSheet("QTextEdit { border: none; font-size: 14px; line-height: 1.8; padding: 16px; }");
    m_highlighter = new MarkdownHighlighter(m_contentEdit->document());
    m_contentStack->addWidget(m_contentEdit);

    m_previewBrowser = new QTextBrowser(this);
    m_previewBrowser->setOpenExternalLinks(true);
    m_previewBrowser->setReadOnly(true);
    m_previewBrowser->setFrameShape(QFrame::NoFrame);
    m_previewBrowser->setStyleSheet("QTextBrowser { border: none; padding: 16px; font-size: 14px; }");
    m_contentStack->addWidget(m_previewBrowser);
    m_contentStack->setCurrentWidget(m_contentEdit);
    mainLayout->addWidget(m_contentStack, 1);

    // 底部栏
    QWidget *bottomBar = new QWidget(this);
    bottomBar->setFixedHeight(44);
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(12, 0, 12, 0);
    bottomLayout->setSpacing(8);

    m_tagCombo = new QComboBox(this);
    m_tagCombo->setFixedHeight(26);
    m_tagCombo->setMinimumWidth(100);
    m_tagCombo->setStyleSheet("QComboBox { border:1px solid palette(mid); border-radius:6px; padding:2px 8px; font-size:11px; }");
    m_tagCombo->addItem(tr("无标签"), "");
    auto *app = ShorthandApplication::instance();
    for (const auto &tag : app->tagManager()->getAllTags()) {
        m_tagCombo->addItem(tag.name, tag.id);
    }
    bottomLayout->addWidget(new DLabel(tr("标签:"), this));
    bottomLayout->addWidget(m_tagCombo);

    QLabel *dateLabel = new QLabel(QDateTime::currentDateTime().toString("MM-dd HH:mm"), this);
    dateLabel->setStyleSheet("color: palette(windowText); font-size: 11px;");
    bottomLayout->addWidget(dateLabel);

    QPushButton *moreBtn = new QPushButton(tr("⋯"), this);
    moreBtn->setFixedSize(26, 26);
    moreBtn->setStyleSheet("QPushButton { background:transparent; border:none; border-radius:6px; font-size:16px; color:palette(windowText); } QPushButton:hover { background:palette(light); }");
    bottomLayout->addWidget(moreBtn);

    bottomLayout->addStretch();

    m_voiceBtn = new QPushButton(tr("🎤 语音输入"), this);
    m_voiceBtn->setFixedHeight(26);
    m_voiceBtn->setStyleSheet("QPushButton { background:palette(light); color:palette(highlight); border:1px solid palette(mid); border-radius:6px; padding:2px 12px; font-size:11px; } QPushButton:hover { background:palette(highlight); }");
    bottomLayout->addWidget(m_voiceBtn);

    mainLayout->addWidget(bottomBar);

    connect(m_voiceBtn, &QPushButton::clicked, this, [this]() {
        if (m_recorder->state() == AudioRecorder::Recording) {
            m_recorder->stopRecording();
        } else {
            m_recorder->startRecording();
        }
    });

    connect(m_recorder, &AudioRecorder::recordingFinished, this, [this](const QString &filePath) {
        m_voiceBtn->setText(tr("🎤 语音输入"));
        auto *app = ShorthandApplication::instance();
        auto *asr = app->asrService();
        if (asr && asr->currentService()) {
            m_voiceBtn->setEnabled(false);
            m_voiceBtn->setText(tr("转写中..."));
            asr->transcribe(filePath, [this, filePath](const AsrResult &result) {
                m_voiceBtn->setEnabled(true);
                m_voiceBtn->setText(tr("🎤 语音输入"));
                if (result.success && !result.text.isEmpty()
                    && result.text != "[BLANK_AUDIO]") {
                    QTextCursor cursor = m_contentEdit->textCursor();
                    cursor.insertText(result.text);
                    m_contentEdit->setTextCursor(cursor);
                    m_modified = true;
                } else {
                    // 静音或空白音频，提示用户
                    QString msg = result.success ? tr("未检测到语音内容，请靠近麦克风说话后重试")
                                                 : result.errorMessage;
                    DMessageBox::warning(this, tr("转写提示"), msg);
                }
                QFile::remove(filePath);
            });
        } else {
            m_voiceBtn->setText(tr("🎤 语音输入"));
        }
    });
    connect(m_recorder, &AudioRecorder::stateChanged, this, [this](AudioRecorder::State state) {
        m_voiceBtn->setText(state == AudioRecorder::Recording ? tr("🔴 录音中...") : tr("🎤 语音输入"));
    });

    clearEditor();
}

void NoteEditorWidget::initConnections()
{
    connect(m_deleteBtn, &QPushButton::clicked, this, &NoteEditorWidget::onDelete);

    connect(m_todoToggleBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentNoteId <= 0) return;
        onSave();
        auto *app = ShorthandApplication::instance();
        NoteData note = app->noteManager()->getNote(m_currentNoteId);
        if (note.id <= 0) return;
        if (note.contentType == "todo") {
            DDialog d(this); d.setTitle(tr("转为笔记")); d.setMessage(tr("确定转为笔记？")); d.addButton(tr("取消")); d.addButton(tr("转为笔记"), true, DDialog::ButtonRecommend);
            if (d.exec() == 1) { app->noteManager()->convertToNote(m_currentNoteId); m_todoToggleBtn->setText(tr("转为待办")); note.contentType = "markdown"; app->noteManager()->updateNote(note); }
        } else {
            DDialog d(this); d.setTitle(tr("转为待办")); d.setMessage(tr("确定转为待办？")); d.addButton(tr("取消")); d.addButton(tr("转为待办"), true, DDialog::ButtonRecommend);
            if (d.exec() == 1) { app->noteManager()->convertToTodo(m_currentNoteId, note.tag == "重要" ? 3 : 2); m_todoToggleBtn->setText(tr("转为笔记")); note.contentType = "todo"; app->noteManager()->updateNote(note); }
        }
        m_modified = true;
    });

    connect(m_tagCombo, &QComboBox::currentTextChanged, this, &NoteEditorWidget::onTagChanged);
    connect(m_titleEdit, &QLineEdit::textChanged, this, [this]() { m_modified = true; });
    connect(m_contentEdit, &QTextEdit::textChanged, this, [this]() { m_modified = true; });
    connect(m_undoBtn, &QToolButton::clicked, this, &NoteEditorWidget::onUndo);
    connect(m_redoBtn, &QToolButton::clicked, this, &NoteEditorWidget::onRedo);
}

void NoteEditorWidget::loadNote(int noteId)
{
    if (m_modified && m_currentNoteId > 0) onSave();
    auto *app = ShorthandApplication::instance();
    NoteData note = app->noteManager()->getNote(noteId);
    if (note.id <= 0) { clearEditor(); return; }

    m_currentNoteId = noteId;
    m_titleEdit->setText(note.title);
    m_contentEdit->setPlainText(note.content);
    int tagIndex = m_tagCombo->findText(note.tag);
    if (tagIndex >= 0) m_tagCombo->setCurrentIndex(tagIndex);
    else m_tagCombo->setCurrentIndex(0);
    m_todoToggleBtn->setText(note.contentType == "todo" ? tr("转为笔记") : tr("转为待办"));
    if (m_previewMode) togglePreview();
    m_modified = false;
    setEnabled(true);
}

void NoteEditorWidget::clearEditor()
{
    m_currentNoteId = -1;
    m_titleEdit->clear();
    m_contentEdit->clear();
    m_tagCombo->setCurrentIndex(0);
    m_modified = false;
    setEnabled(false);
    if (m_previewMode) togglePreview();
}

void NoteEditorWidget::onSave()
{
    if (m_currentNoteId <= 0) return;
    auto *app = ShorthandApplication::instance();
    NoteData note = app->noteManager()->getNote(m_currentNoteId);
    note.title = m_titleEdit->text();
    if (note.title.isEmpty()) note.title = tr("无标题笔记");
    note.content = m_contentEdit->toPlainText();
    note.tag = m_tagCombo->currentText();
    if (note.tag == tr("无标签")) note.tag = "";
    note.modificationDatetime = QDateTime::currentSecsSinceEpoch();
    if (app->noteManager()->updateNote(note)) { m_modified = false; }
}

void NoteEditorWidget::onDelete()
{
    if (m_currentNoteId <= 0) return;
    DDialog d(this);
    d.setTitle(tr("确认删除"));
    d.setMessage(tr("确定要删除这条笔记吗？可在回收站中恢复。"));
    d.addButton(tr("取消"));
    d.addButton(tr("删除"), true, DDialog::ButtonWarning);
    if (d.exec() == 1) {
        ShorthandApplication::instance()->noteManager()->deleteNote(m_currentNoteId);
        clearEditor();
    }
}

void NoteEditorWidget::onTagChanged() { m_modified = true; }

void NoteEditorWidget::togglePreview()
{
    m_previewMode = m_previewBtn->isChecked();
    if (m_previewMode) {
        QString md = m_contentEdit->toPlainText();
        QString html = md;
        html.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
        html.replace(QRegularExpression("^# (.+)$", QRegularExpression::MultilineOption), "<h1>\\1</h1>");
        html.replace(QRegularExpression("^## (.+)$", QRegularExpression::MultilineOption), "<h2>\\1</h2>");
        html.replace(QRegularExpression("^### (.+)$", QRegularExpression::MultilineOption), "<h3>\\1</h3>");
        html.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<b>\\1</b>");
        html.replace(QRegularExpression("\\*(.+?)\\*"), "<i>\\1</i>");
        html.replace(QRegularExpression("`(.+?)`"), "<code>\\1</code>");
        html.replace(QRegularExpression("\\[(.+?)\\]\\((.+?)\\)"), "<a href='\\2'>\\1</a>");
        html.replace(QRegularExpression("^- (.+)$", QRegularExpression::MultilineOption), "<li>\\1</li>");
        html.replace("\n", "<br>");
        m_previewBrowser->setHtml(html);
        m_contentStack->setCurrentWidget(m_previewBrowser);
        m_previewBtn->setText(tr("编辑"));
    } else {
        m_contentStack->setCurrentWidget(m_contentEdit);
        m_previewBtn->setText(tr("预览"));
    }
}

void NoteEditorWidget::onScreenshot() { /* handled by signal connection */ }

void NoteEditorWidget::doAiComplete(const QString &systemPrompt, const QString &userText,
                                      std::function<void(const QString &)> onResult)
{
    auto *app = ShorthandApplication::instance();
    auto *ai = app->aiService();
    if (!ai) { DDialog d(this); d.setTitle(tr("AI 未配置")); d.setMessage(tr("请在设置中配置 API Key")); d.addButton(tr("确定")); d.exec(); return; }
    QProgressDialog *progress = new QProgressDialog(tr("AI 处理中..."), QString(), 0, 0, this);
    progress->setWindowTitle(tr("AI 助手"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setCancelButton(nullptr);
    progress->show();
    AiCompletionRequest req;
    req.messages.append({"system", systemPrompt});
    req.messages.append({"user", userText});
    req.temperature = 0.7;
    req.maxTokens = 2048;
    ai->complete(req, [this, progress, onResult](const AiCompletionResult &result) {
        progress->close(); progress->deleteLater();
        if (result.success) onResult(result.content);
        else { DDialog d(this); d.setTitle(tr("AI 处理失败")); d.setMessage(result.errorMessage); d.addButton(tr("确定")); d.exec(); }
    });
}

void NoteEditorWidget::onAiAction(const QString &action)
{
    QString selected = m_contentEdit->textCursor().selectedText();
    QString fullContent = m_contentEdit->toPlainText();
    QString inputText = selected.isEmpty() ? fullContent : selected;
    if (inputText.trimmed().isEmpty()) { DDialog d(this); d.setTitle(tr("提示")); d.setMessage(tr("请先输入内容")); d.addButton(tr("确定")); d.exec(); return; }

    QString sysPrompt, actionLabel;
    if (action == "polish") { sysPrompt = tr("你是一个专业润色助手，优化文本使其更流畅专业。"); actionLabel = tr("润色"); }
    else if (action == "expand") { sysPrompt = tr("基于以下内容扩写，增加细节。"); actionLabel = tr("扩写"); }
    else if (action == "translate_en") { sysPrompt = tr("Translate Chinese to English."); actionLabel = tr("中译英"); }
    else if (action == "translate_zh") { sysPrompt = tr("Translate English to Chinese."); actionLabel = tr("英译中"); }
    else if (action == "summarize") { sysPrompt = tr("生成简洁摘要，不超过200字。"); actionLabel = tr("摘要"); }
    else if (action == "extract_todos") {
        auto *app = ShorthandApplication::instance(); auto *ai = app->aiService();
        if (!ai) return;
        QProgressDialog *progress = new QProgressDialog(tr("提取待办中..."), QString(), 0, 0, this);
        progress->setWindowTitle(tr("AI 助手")); progress->setWindowModality(Qt::WindowModal); progress->setCancelButton(nullptr); progress->show();
        ai->extractTodos(inputText, [this, progress](const QList<QPair<QString, int>> &todos) {
            progress->close(); progress->deleteLater();
            if (todos.isEmpty()) { DDialog d(this); d.setTitle(tr("提取结果")); d.setMessage(tr("未识别出待办事项")); d.addButton(tr("确定")); d.exec(); return; }
            auto *app = ShorthandApplication::instance(); int count = 0;
            for (const auto &todo : todos) { TodoData td; td.title = todo.first; td.priority = todo.second; td.creationDatetime = QDateTime::currentSecsSinceEpoch(); td.modificationDatetime = td.creationDatetime; if (app->todoManager()->createTodo(td) > 0) count++; }
            DDialog d(this); d.setTitle(tr("提取完成")); d.setMessage(tr("成功提取 %1 条待办").arg(count)); d.addButton(tr("确定")); d.exec();
        });
        return;
    } else return;

    doAiComplete(sysPrompt, inputText, [this, selected, actionLabel](const QString &result) {
        QTextCursor cursor = m_contentEdit->textCursor();
        if (!cursor.hasSelection()) m_contentEdit->append("\n\n--- " + actionLabel + tr("结果 ---\n") + result);
        else cursor.insertText(result);
        m_modified = true;
    });
}