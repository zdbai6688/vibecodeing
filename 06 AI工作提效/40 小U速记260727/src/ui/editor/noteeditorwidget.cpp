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
#include <QTextBlock>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextFormat>
#include <QProgressDialog>
#include <QToolButton>
#include <QPair>
#include <QFontDatabase>
#include <QColorDialog>
#include <DGuiApplicationHelper>
#include <QStandardPaths>
#include <QDir>
#include <QRandomGenerator>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QKeyEvent>
#include <QUrl>
#include <QRegularExpression>

NoteEditorWidget::NoteEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    qInfo() << "[Editor] 构造开始";
    m_screenshotMgr = new ScreenshotManager(this);
    qInfo() << "[Editor] ScreenshotManager";
    m_recorder = new AudioRecorder(this);
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    m_autoSaveTimer->setInterval(800);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &NoteEditorWidget::onSave);
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

    QToolButton *colorBtn = makeBtn("●", tr("文字颜色"));
    colorBtn->setStyleSheet("QToolButton { background:transparent; border:none; border-radius:6px; font-size:16px; color:palette(highlight); } QToolButton:hover { background:palette(light); }");
    colorBtn->setToolTip(tr("文字颜色（作用于选中文字）"));

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
    QToolButton *ssBtn = makeBtn("📷", tr("截图插入"));
    toolbar->addStretch();

    // ─── 富文本格式：作用于选中文字（不再插入 * 号等 Markdown 标记） ───
    auto applyCharFormat = [this](const std::function<void(QTextCharFormat &)> &mutator) {
        QTextCursor cursor = m_contentEdit->textCursor();
        if (cursor.hasSelection()) {
            QTextCharFormat fmt = cursor.charFormat();
            mutator(fmt);
            cursor.mergeCharFormat(fmt);
        } else {
            QTextCharFormat fmt = m_contentEdit->currentCharFormat();
            mutator(fmt);
            m_contentEdit->setCurrentCharFormat(fmt);
        }
        m_contentEdit->setFocus();
        m_modified = true;
    };

    // 记住上次选择的颜色：下次打开取色器时直接沿用，避免每次都要重新选色（TC03）
    QColor lastTextColor;
    connect(colorBtn, &QToolButton::clicked, this, [this, applyCharFormat, &lastTextColor]() {
        QColor defaultColor = lastTextColor.isValid()
                ? lastTextColor : palette().color(QPalette::WindowText);
        QColor c = QColorDialog::getColor(defaultColor, this, tr("选择文字颜色"));
        if (c.isValid()) {
            lastTextColor = c;
            applyCharFormat([c](QTextCharFormat &fmt) { fmt.setForeground(c); });
            m_contentEdit->setFocus();
        }
    });

    // 字体族：全部中文字体，供选择
    m_fontCombo->clear();
    m_fontCombo->addItem(tr("默认字体"), QString());
    const QStringList families = QFontDatabase().families(QFontDatabase::SimplifiedChinese);
    for (const QString &family : families) {
        if (family == tr("默认字体")) continue;
        m_fontCombo->addItem(family, family);
    }
    connect(m_fontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, applyCharFormat](int index) {
        const QString family = m_fontCombo->itemData(index).toString();
        applyCharFormat([family](QTextCharFormat &fmt) {
            if (family.isEmpty()) {
                fmt.clearProperty(QTextFormat::FontFamilies);
            } else {
                fmt.setFontFamilies({family});
            }
        });
    });

    // 字号：作用于选中文字
    connect(m_sizeCombo, &QComboBox::currentTextChanged, this, [this, applyCharFormat](const QString &sizeText) {
        bool ok = false;
        double pt = sizeText.toDouble(&ok);
        if (!ok || pt <= 0) return;
        applyCharFormat([pt](QTextCharFormat &fmt) { fmt.setFontPointSize(pt); });
    });

    connect(m_boldBtn, &QToolButton::clicked, this, [this, applyCharFormat]() {
        QTextCharFormat cur = m_contentEdit->textCursor().hasSelection()
                ? m_contentEdit->textCursor().charFormat()
                : m_contentEdit->currentCharFormat();
        const bool makeBold = cur.fontWeight() != QFont::Bold;
        applyCharFormat([makeBold](QTextCharFormat &fmt) {
            fmt.setFontWeight(makeBold ? QFont::Bold : QFont::Normal);
        });
        m_boldBtn->setChecked(makeBold);
    });
    connect(m_italicBtn, &QToolButton::clicked, this, [this, applyCharFormat]() {
        QTextCharFormat cur = m_contentEdit->textCursor().hasSelection()
                ? m_contentEdit->textCursor().charFormat()
                : m_contentEdit->currentCharFormat();
        const bool makeItalic = !cur.fontItalic();
        applyCharFormat([makeItalic](QTextCharFormat &fmt) { fmt.setFontItalic(makeItalic); });
        m_italicBtn->setChecked(makeItalic);
    });
    connect(m_underlineBtn, &QToolButton::clicked, this, [this, applyCharFormat]() {
        QTextCharFormat cur = m_contentEdit->textCursor().hasSelection()
                ? m_contentEdit->textCursor().charFormat()
                : m_contentEdit->currentCharFormat();
        const bool makeUnderline = !cur.fontUnderline();
        applyCharFormat([makeUnderline](QTextCharFormat &fmt) { fmt.setFontUnderline(makeUnderline); });
        m_underlineBtn->setChecked(makeUnderline);
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
    connect(ssBtn, &QToolButton::clicked, this, &NoteEditorWidget::onInsertScreenshot);

    // ─── 预览切换按钮（此前只声明未创建，导致预览功能完全不可用） ───
    toolbar->addSpacing(8);
    m_previewBtn = new QPushButton(tr("预览"), this);
    m_previewBtn->setCheckable(true);
    m_previewBtn->setFixedHeight(26);
    m_previewBtn->setStyleSheet("QPushButton { border:1px solid palette(mid); border-radius:6px; padding:2px 12px; font-size:12px; color:palette(windowText); background:transparent; } QPushButton:hover { border-color:palette(highlight); color:palette(highlight); } QPushButton:checked { background:palette(highlight); color:palette(highlightedText); border-color:palette(highlight); }");
    toolbar->addWidget(m_previewBtn);
    connect(m_previewBtn, &QPushButton::clicked, this, &NoteEditorWidget::togglePreview);

    mainLayout->addWidget(toolbarWidget);
}

void NoteEditorWidget::onInsertImage()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择图片"), QString(), tr("图片 (*.png *.jpg *.jpeg *.gif *.bmp)"));
    if (!filePath.isEmpty()) {
        QString stored = saveImageToAppData(filePath);
        m_contentEdit->textCursor().insertText(QString("![](%1)").arg(stored));
        m_modified = true;
    }
}

void NoteEditorWidget::onInsertScreenshot()
{
    // 调用系统截图工具，截图完成后插入笔记（ScreenshotManager 信号在 initConnections 中连接）
    m_screenshotMgr->captureRegion();
}

bool NoteEditorWidget::onPasteImageFromClipboard()
{
    const QClipboard *clipboard = QApplication::clipboard();
    if (!clipboard || !clipboard->mimeData()->hasImage()) return false;
    QPixmap pix = clipboard->pixmap();
    if (pix.isNull()) return false;

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/images";
    QDir().mkpath(dir);
    const QString path = dir + "/img_" + QString::number(QDateTime::currentSecsSinceEpoch())
                         + "_" + QString::number(QRandomGenerator::global()->bounded(100000)) + ".png";
    if (pix.save(path, "PNG")) {
        m_contentEdit->textCursor().insertText(QString("![](%1)").arg(path));
        m_modified = true;
        return true;
    }
    return false;
}

// 把图片复制进应用数据目录，避免外部文件被移动/删除后笔记图片失效
QString NoteEditorWidget::saveImageToAppData(const QString &srcPath) const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/images";
    QDir().mkpath(dir);
    QString suffix = QFileInfo(srcPath).suffix();
    if (suffix.isEmpty()) suffix = "png";
    const QString dest = dir + "/img_" + QString::number(QDateTime::currentSecsSinceEpoch())
                         + "_" + QString::number(QRandomGenerator::global()->bounded(100000)) + "." + suffix;
    if (QFile::exists(srcPath) && QFile::copy(srcPath, dest)) return dest;
    return srcPath; // 复制失败时退回原路径
}

bool NoteEditorWidget::eventFilter(QObject *watched, QEvent *event)
{
    // Ctrl+V：剪贴板含图片时保存并插入笔记（含截图工具“复制到剪贴板”场景）
    if (watched == m_contentEdit && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_V && (ke->modifiers() & Qt::ControlModifier)) {
            if (onPasteImageFromClipboard()) return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void NoteEditorWidget::onUndo() { m_contentEdit->undo(); }
void NoteEditorWidget::onRedo() { m_contentEdit->redo(); }

void NoteEditorWidget::updateWordCount()
{
    if (!m_wordCountLabel || !m_contentEdit) return;
    const QString text = m_contentEdit->toPlainText();
    int count = 0;
    for (const QChar &c : text) {
        if (!c.isSpace()) ++count;   // 统计非空白字符（中英文均计 1 字）
    }
    m_wordCountLabel->setText(tr("%1 字").arg(count));
}

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
    m_previewBrowser->setStyleSheet(
        "QTextBrowser { border: none; padding: 16px;"
        " font-size: 14px; color: palette(windowText);"
        " selection-background-color: palette(highlight);"
        " selection-color: palette(highlightedText); }");
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

    m_wordCountLabel = new QLabel(tr("0 字"), this);
    m_wordCountLabel->setStyleSheet("color: palette(placeholderText); font-size: 11px;");
    bottomLayout->addWidget(m_wordCountLabel);

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
    connect(m_titleEdit, &QLineEdit::textChanged, this, [this]() { m_modified = true; m_autoSaveTimer->start(); });
    connect(m_contentEdit, &QTextEdit::textChanged, this, [this]() { m_modified = true; });
    connect(m_contentEdit, &QTextEdit::textChanged, this, &NoteEditorWidget::updateWordCount);
    // 光标移动时同步工具栏按钮状态（m_contentEdit 在 initUI 中已创建）
    connect(m_contentEdit, &QTextEdit::cursorPositionChanged, this, [this]() {
        if (!m_contentEdit) return;
        const QTextCharFormat fmt = m_contentEdit->currentCharFormat();
        if (m_boldBtn) m_boldBtn->setChecked(fmt.fontWeight() == QFont::Bold);
        if (m_italicBtn) m_italicBtn->setChecked(fmt.fontItalic());
        if (m_underlineBtn) m_underlineBtn->setChecked(fmt.fontUnderline());
    });
    connect(m_undoBtn, &QToolButton::clicked, this, &NoteEditorWidget::onUndo);
    connect(m_redoBtn, &QToolButton::clicked, this, &NoteEditorWidget::onRedo);

    // 截图插入：截图成功后复制到应用目录并插入（此前 ScreenshotManager 从未连接，截图功能无效）
    connect(m_screenshotMgr, &ScreenshotManager::screenshotTaken, this, [this](const QString &filePath) {
        QString stored = saveImageToAppData(filePath);
        m_contentEdit->textCursor().insertText(QString("![](%1)").arg(stored));
        m_modified = true;
    });
    connect(m_screenshotMgr, &ScreenshotManager::screenshotFailed, this, [this](const QString &errorMessage) {
        DDialog d(this);
        d.setTitle(tr("截图失败"));
        d.setMessage(errorMessage);
        d.addButton(tr("确定"));
        d.exec();
    });

    // Ctrl+V 粘贴图片支持
    m_contentEdit->installEventFilter(this);
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
    updateWordCount();
}

void NoteEditorWidget::clearEditor()
{
    m_currentNoteId = -1;
    m_titleEdit->clear();
    m_contentEdit->clear();
    m_tagCombo->setCurrentIndex(0);
    m_modified = false;
    setEnabled(false);
    updateWordCount();
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
    // 预览按钮为真实控件（此前只声明未创建，预览功能完全不可用）
    m_previewMode = m_previewBtn->isChecked();
    if (m_previewMode) {
        m_previewBrowser->setHtml(renderPreviewHtml());
        m_contentStack->setCurrentWidget(m_previewBrowser);
        m_previewBtn->setText(tr("编辑"));
    } else {
        m_contentStack->setCurrentWidget(m_contentEdit);
        m_previewBtn->setText(tr("预览"));
    }
}

QString NoteEditorWidget::renderPreviewHtml() const
{
    // 以富文本文档副本为基础：保留工具栏设置的粗体/颜色/字号，同时把
    // 手动输入的 Markdown 行级标记（# 标题 / - 列表 / > 引用）转换为对应格式，
    // 再把行内标记（加粗/斜体/代码/图片/链接）转为 HTML。深拷贝保证不改动编辑区。
    QTextDocument docCopy;
    docCopy.setDefaultFont(m_contentEdit->font());
    docCopy.setHtml(m_contentEdit->document()->toHtml());
    QTextCursor cur(&docCopy);
    cur.beginEditBlock();

    for (QTextBlock block = docCopy.begin(); block.isValid(); block = block.next()) {
        QString text = block.text();
        QString stripped = text.trimmed();
        QTextCharFormat bf = block.charFormat();
        int prefixLen = 0;

        if (stripped.startsWith("### ")) {
            bf.setFontPointSize(15); bf.setFontWeight(QFont::Bold); prefixLen = 4;
        } else if (stripped.startsWith("## ")) {
            bf.setFontPointSize(17); bf.setFontWeight(QFont::Bold); prefixLen = 3;
        } else if (stripped.startsWith("# ")) {
            bf.setFontPointSize(20); bf.setFontWeight(QFont::Bold); prefixLen = 2;
        } else if (stripped.startsWith("> ")) {
            bf.setForeground(QColor(0x8e, 0x8e, 0x93));
            prefixLen = 2;
        }
        // 列表项（- / *）保留标记，交由下方 HTML 正则包成 <ul><li>，避免转换后无法识别

        if (prefixLen > 0) {
            QTextCursor bc(&docCopy);
            bc.setPosition(block.position());
            bc.setPosition(block.position() + prefixLen, QTextCursor::KeepAnchor);
            bc.removeSelectedText();
            QTextCursor bfCur(&docCopy);
            bfCur.setPosition(block.position());
            bfCur.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
            bfCur.setCharFormat(bf);
        }
    }
    cur.endEditBlock();

    QString html = docCopy.toHtml();

    // 列表：把转换后的 - 项包成 <ul><li>（逐项处理，避免嵌套错乱）
    QStringList lines;
    const QStringList rawLines = html.split('\n');
    bool inUl = false;
    for (QString line : rawLines) {
        QRegularExpressionMatch m = QRegularExpression("^\\s*(<p[^>]*>)\\s*[-*] (.+?)(</p>)\\s*$",
                                                       QRegularExpression::MultilineOption).match(line);
        if (m.hasMatch()) {
            if (!inUl) { lines << "<ul>"; inUl = true; }
            lines << m.captured(1) + "<li>" + m.captured(2) + "</li>" + m.captured(3);
        } else {
            if (inUl) { lines << "</ul>"; inUl = false; }
            lines << line;
        }
    }
    if (inUl) lines << "</ul>";
    html = lines.join('\n');

    // 行内：图片 / 链接 / 加粗 / 斜体 / 行内代码
    html.replace(QRegularExpression("!\\[([^\\]]*)\\]\\(([^\\)]+)\\)"),
                 "<img src='file://\\2' alt='\\1' style='max-width:100%;' />");
    html.replace(QRegularExpression("\\[([^\\]]+)\\]\\(([^\\)]+)\\)"),
                 "<a href='file://\\2'>\\1</a>");
    html.replace(QRegularExpression("\\*\\*([^*]+)\\*\\*"), "<b>\\1</b>");
    html.replace(QRegularExpression("\\*([^*]+)\\*"), "<i>\\1</i>");
    html.replace(QRegularExpression("`([^`]+)`"), "<code>\\1</code>");

    // 代码块
    html.replace(QRegularExpression("```\n?([^`]*)```"), "<pre>\\1</pre>");

    return html;
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