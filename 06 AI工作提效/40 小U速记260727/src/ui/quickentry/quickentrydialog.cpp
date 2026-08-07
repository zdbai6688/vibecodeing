// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "quickentrydialog.h"
#include "application/shorthandapplication.h"
#include "core/notemanager.h"
#include "core/todomanager.h"
#include "core/tagmanager.h"
#include "ui/desktop/desktopmodemanager.h"
#include "ui/edgeautohide.h"
#include "ui/tray/traymanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QGraphicsDropShadowEffect>
#include <QScreen>
#include <QGuiApplication>
#include <QSettings>
#include <DLabel>
#include <DGuiApplicationHelper>

// 每个紧凑窗口实例的独立编号（草稿按实例隔离，多窗口互不干扰）
static int s_nextInstanceId = 1;
// 上一个已关闭窗口遗留的草稿路径，新窗口创建时恢复（未保存内容找回）
static QString s_pendingDraftPath;

static const char *COMPACT_STYLE = R"(
    QWidget#compactContainer {
        background: palette(window);
        border: 1px solid palette(mid);
        border-radius: 12px;
    }
    QTextEdit#compactInput {
        border: none;
        font-size: 14px;
        background: transparent;
        padding: 6px;
    }
    QPushButton#compactBtn {
        background: transparent;
        border: none;
        border-radius: 8px;
        font-size: 13px;
        padding: 4px 10px;
        color: palette(placeholderText);
    }
    QPushButton#compactBtn:hover {
        background: palette(light);
        color: palette(highlight);
    }
    QPushButton#compactSave {
        background: palette(highlight);
        color: palette(highlightedText);
        border: none;
        border-radius: 8px;
        font-size: 13px;
        padding: 4px 16px;
        font-weight: 600;
    }
    QPushButton#compactSave:hover {
        background: palette(dark);
    }
    QPushButton#pinBtn {
        background: transparent;
        border: 1px solid palette(mid);
        border-radius: 8px;
        font-size: 12px;
        padding: 4px 10px;
        color: palette(placeholderText);
    }
    QPushButton#pinBtn:hover {
        background: palette(light);
        color: palette(highlight);
        border-color: palette(highlight);
    }
)";

QuickEntryDialog::QuickEntryDialog(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(COMPACT_STYLE);

    // Load continuous add preference
    QSettings settings;
    m_continuousAdd = settings.value("desktop/continuous_add", false).toBool();

    // 贴边自动隐藏助手：必须在构造时创建，否则 m_edgeHide 为野指针，
    // centerOnScreen()/鼠标事件里访问会段错误（打开快速录入窗即崩溃）
    m_edgeHide = new EdgeAutoHide(this, this);

    m_instanceId = s_nextInstanceId++;

    initUI();
    initConnections();

    // 恢复上一个已关闭窗口遗留的草稿（未保存内容找回）
    restorePendingDraft();
}

void QuickEntryDialog::initUI()
{
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    // 统一单页窗口（TC14 二轮）：取消「紧凑/全屏」双尺寸切换，输入随内容自动增高
    m_page = new QWidget(this);
    m_page->setObjectName("compactContainer");

    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 2);
    auto *helper = DGuiApplicationHelper::instance();
    bool isDark = helper && helper->themeType() == DGuiApplicationHelper::DarkType;
    shadow->setColor(isDark ? QColor(0, 0, 0, 120) : QColor(0, 0, 0, 40));
    m_page->setGraphicsEffect(shadow);

    QVBoxLayout *layout = new QVBoxLayout(m_page);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);

    // Top row: drag handle + continuous switch + close
    QHBoxLayout *topRow = new QHBoxLayout();
    m_dragBar = new DragHandle(m_edgeHide, this);
    m_dragBar->setObjectName("compactBtn");
    topRow->addWidget(m_dragBar);

    // 连续新增开关（TC14 二轮：说明更明确，避免用户不知道「连续」作用）
    DLabel *contLabel = new DLabel(tr("连续添加"), this);
    contLabel->setStyleSheet("font-size: 11px; color: palette(placeholderText);");
    contLabel->setToolTip(tr("开启后保存不关闭窗口，可连续录入多条"));
    topRow->addWidget(contLabel);
    m_continuousSwitch = new DSwitchButton(this);
    m_continuousSwitch->setChecked(m_continuousAdd);
    m_continuousSwitch->setFixedSize(36, 20);
    m_continuousSwitch->setToolTip(tr("连续添加：保存后不关闭窗口，可直接继续录入下一条"));
    topRow->addWidget(m_continuousSwitch);

    topRow->addStretch();
    QPushButton *closeBtn = new QPushButton(tr("✕"), this);
    closeBtn->setObjectName("compactClose");
    closeBtn->setFixedSize(28, 28);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setToolTip(tr("关闭（Esc）"));
    closeBtn->setStyleSheet(
        "QPushButton#compactClose { background: transparent; border: none; border-radius: 14px;"
        " font-size: 14px; color: palette(windowText); }"
        "QPushButton#compactClose:hover { background: rgba(128,128,128,0.15); }");
    topRow->addWidget(closeBtn);
    layout->addLayout(topRow);

    // Content：单编辑器，随内容自动增高（最多 3 行高度，超出滚动）
    m_contentEdit = new QTextEdit(this);
    m_contentEdit->setObjectName("compactInput");
    m_contentEdit->setPlaceholderText(tr("快速记录...\n支持 #标签 和 !高/中/低 优先级，Enter 保存，Ctrl+Enter 换行"));
    m_contentEdit->setAcceptRichText(false);
    m_contentEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_contentEdit->setMinimumHeight(64);
    m_contentEdit->setMaximumHeight(140);
    layout->addWidget(m_contentEdit, 1);
    // TC14 五轮：QTextEdit 会自行消费 Enter 插入换行，需在事件过滤器里拦截，
    // 否则父窗口 keyPressEvent 收不到 → Enter 保存失效
    m_contentEdit->installEventFilter(this);

    // Bottom row: voice, screenshot, hint, save, pin-to-desktop
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(4);
    QPushButton *voiceBtn = new QPushButton(tr("🎤"), this);
    voiceBtn->setObjectName("compactBtn");
    voiceBtn->setFixedSize(30, 30);
    voiceBtn->setToolTip(tr("语音"));
    QPushButton *ssBtn = new QPushButton(tr("📷"), this);
    ssBtn->setObjectName("compactBtn");
    ssBtn->setFixedSize(30, 30);
    ssBtn->setToolTip(tr("截图"));
    bottomRow->addWidget(voiceBtn);
    bottomRow->addWidget(ssBtn);

    m_hintLabel = new DLabel(tr("Enter 保存 · Ctrl+Enter 换行"), this);
    m_hintLabel->setStyleSheet("font-size: 10px; color: palette(placeholderText);");
    bottomRow->addWidget(m_hintLabel);
    bottomRow->addStretch();

    m_pinToDesktopBtn = new QPushButton(tr("📌 贴到桌面"), this);
    m_pinToDesktopBtn->setObjectName("pinBtn");
    m_pinToDesktopBtn->setFixedHeight(30);
    m_pinToDesktopBtn->setToolTip(tr("保存并贴到桌面便签"));
    bottomRow->addWidget(m_pinToDesktopBtn);

    m_saveBtn = new QPushButton(tr("保存"), this);
    m_saveBtn->setObjectName("compactSave");
    m_saveBtn->setFixedHeight(30);
    bottomRow->addWidget(m_saveBtn);
    layout->addLayout(bottomRow);

    outerLayout->addWidget(m_page);

    connect(voiceBtn, &QPushButton::clicked, this, &QuickEntryDialog::onVoiceInput);
    connect(ssBtn, &QPushButton::clicked, this, &QuickEntryDialog::onScreenshot);
    connect(m_saveBtn, &QPushButton::clicked, this, &QuickEntryDialog::onSave);
    connect(m_pinToDesktopBtn, &QPushButton::clicked, this, &QuickEntryDialog::onPinToDesktop);
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        // 关闭：未保存内容以本窗口草稿保留，供下一个新窗口恢复
        dismissWithDraft();
    });
    connect(m_continuousSwitch, &DSwitchButton::toggled, this, [this](bool checked) {
        m_continuousAdd = checked;
        QSettings().setValue("desktop/continuous_add", checked);
    });
}

void QuickEntryDialog::initConnections()
{
    // No additional connections needed
}

void QuickEntryDialog::setPasteToDesktopMode(bool on)
{
    m_waitingForPin = on;
    if (m_pinToDesktopBtn) {
        m_pinToDesktopBtn->setVisible(on);
    }
}

void QuickEntryDialog::showCompact()
{
    // Check for saved draft（本窗口独立草稿）
    QString draftPath = this->draftPath();
    QFile draftFile(draftPath);
    if (draftFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString draft = QString::fromUtf8(draftFile.readAll()).trimmed();
        draftFile.close();
        if (!draft.isEmpty()) {
            m_contentEdit->setPlainText(draft);
        }
        // Clear draft after restore
        QFile::remove(draftPath);
    }

    setFixedSize(440, 240);
    centerOnScreen();
    show();
    activateWindow();
    m_contentEdit->setFocus();
    m_hidden = false;
    m_ghostState = false;
    setWindowOpacity(1.0);
}

void QuickEntryDialog::showFull() {}

void QuickEntryDialog::centerOnScreen()
{
    if (m_edgeHide) {
        m_edgeHide->cancelDock();
    }
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    QRect screenGeo = screen->availableGeometry();
    QPoint pos = screenGeo.center() - rect().center();
    // 多窗口级联偏移，避免同时打开时完全重叠
    pos += QPoint((m_cascadeIndex % 5) * 32, (m_cascadeIndex / 5) * 32);
    move(pos);
}

void QuickEntryDialog::setFocus()
{
    if (m_hidden) {
        showCompact();
    }
    QWidget::setFocus();
    m_contentEdit->setFocus();
    m_ghostState = false;
    setWindowOpacity(1.0);
}

void QuickEntryDialog::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    centerOnScreen();
    m_contentEdit->setFocus();
}

void QuickEntryDialog::enterGhostState()
{
    if (m_ghostState || m_hidden) return;
    m_ghostState = true;

    // Save draft
    QString content = m_contentEdit->toPlainText();
    if (!content.trimmed().isEmpty()) {
        // 本窗口独立草稿，避免多窗口互相覆盖
        QString draftDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/drafts";
        QDir().mkpath(draftDir);
        QFile draftFile(draftPath());
        if (draftFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            draftFile.write(content.toUtf8());
            draftFile.close();
        }
    }

    // Lower opacity and deactivate
    setWindowOpacity(0.6);
    lower();
}

void QuickEntryDialog::leaveGhostState()
{
    if (!m_ghostState) return;
    m_ghostState = false;
    setWindowOpacity(1.0);
    raise();
    activateWindow();
}

void QuickEntryDialog::updateHint() {}

bool QuickEntryDialog::eventFilter(QObject *watched, QEvent *event)
{
    // 拦截 QTextEdit 上的 Enter/Ctrl+Enter（QTextEdit 会先消费 Enter 换行，父窗口收不到）
    if (watched == m_contentEdit && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (ke->modifiers() & Qt::ControlModifier) {
                m_contentEdit->insertPlainText("\n");
            } else {
                onSave();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void QuickEntryDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (event->modifiers() & Qt::ControlModifier) {
            m_contentEdit->insertPlainText("\n");
        } else {
            onSave();
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        onDiscard();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void QuickEntryDialog::focusOutEvent(QFocusEvent *event)
{
    // Enter ghost state if there's unsaved content
    QString content = m_contentEdit->toPlainText();
    if (!content.trimmed().isEmpty() && !m_hidden) {
        enterGhostState();
    }
    QWidget::focusOutEvent(event);
}

void QuickEntryDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange) {
        if (!isActiveWindow() && !m_hidden) {
            QString content = m_contentEdit->toPlainText();
            if (!content.trimmed().isEmpty()) {
                enterGhostState();
            }
        }
    }
    QWidget::changeEvent(event);
}

void QuickEntryDialog::mousePressEvent(QMouseEvent *event)
{
    // 点击窗口空白区域即可拖动窗口（按钮/输入框等子控件会先消费事件）
    if (event->button() == Qt::LeftButton && !m_hidden && m_edgeHide) {
        m_edgeHide->startDrag(event->globalPosition().toPoint());
        grabMouse();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void QuickEntryDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (m_edgeHide) {
        m_edgeHide->dragTo(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void QuickEntryDialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_edgeHide) {
        releaseMouse();
        m_edgeHide->endDrag();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

QString QuickEntryDialog::parseTags(const QString &text, QStringList &outTags)
{
    static QRegularExpression tagRe("#([\\p{L}\\w_-]+)");
    QString result = text;
    QRegularExpressionMatchIterator it = tagRe.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString tag = match.captured(1);
        if (!outTags.contains(tag)) {
            outTags.append(tag);
        }
        result.remove(match.capturedStart(), match.capturedLength());
    }
    return result.trimmed();
}

int QuickEntryDialog::parsePriority(const QString &text, QString &outText)
{
    if (text.contains("!高") || text.contains("!紧急")) {
        outText = text;
        outText.remove(QRegularExpression("![\\w]+"));
        return 3;
    }
    if (text.contains("!中")) {
        outText = text;
        outText.remove(QRegularExpression("![\\w]+"));
        return 2;
    }
    if (text.contains("!低")) {
        outText = text;
        outText.remove(QRegularExpression("![\\w]+"));
        return 1;
    }
    outText = text;
    return 0;
}

void QuickEntryDialog::onVoiceInput()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/audio";
    QDir().mkpath(dir);
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择录音文件"),
                                                     dir, tr("音频文件 (*.wav *.mp3 *.ogg *.m4a)"));
    if (!filePath.isEmpty()) {
        QString text = tr("[语音: %1]").arg(QFileInfo(filePath).fileName());
        m_contentEdit->insertPlainText(text);
    }
}

void QuickEntryDialog::onScreenshot()
{
    QString ts = QString::number(QDateTime::currentSecsSinceEpoch());
    QString text = tr("[截图: %1.png]").arg(ts);
    m_contentEdit->insertPlainText(text);
}

void QuickEntryDialog::onSave()
{
    QString content = m_contentEdit->toPlainText().trimmed();

    if (content.isEmpty()) {
        hide();
        m_hidden = true;
        emit dismissed();
        return;
    }

    QStringList tags;
    QString cleaned = parseTags(content, tags);
    int priority = parsePriority(cleaned, cleaned);
    content = cleaned;

    auto *app = ShorthandApplication::instance();
    NoteData note;
    note.title = content.left(60).section('\n', 0, 0);
    note.content = content;
    note.contentType = priority > 0 ? "todo" : "markdown";
    note.creationDatetime = QDateTime::currentSecsSinceEpoch();
    note.modificationDatetime = note.creationDatetime;

    if (!tags.isEmpty()) {
        note.tag = tags.join(", ");
        for (const QString &tagName : tags) {
            if (!app->tagManager()->allTagNames().contains(tagName, Qt::CaseInsensitive)) {
                app->tagManager()->createTag(tagName, "#1890FF");
            }
        }
    }

    int id = app->noteManager()->createNote(note);
    if (priority > 0 && id > 0) {
        app->noteManager()->convertToTodo(id, priority);
        // 使用 TodoManager 更新多标签
        app->todoManager()->setTodoTags(id, tags);
    }

    m_lastSavedNoteId = id;

    // Handle pin-to-desktop if waiting
    if (m_waitingForPin && id > 0) {
        app->desktopModeManager()->pinNoteToDesktop(id);
        if (!app->desktopModeManager()->isDesktopMode()) {
            app->desktopModeManager()->enterDesktopMode();
        }
        m_waitingForPin = false;
        // TC14 二轮：贴到桌面后明确提示便签位置与返回方式，避免用户找不到
        app->trayManager()->showMessage(
            tr("已贴到桌面"),
            tr("便签已显示在桌面右侧，可拖动调整位置；点击系统托盘图标可管理/返回主窗口"));
    }

    if (m_continuousAdd) {
        // Clear but don't close
        m_contentEdit->clear();
        m_contentEdit->setFocus();
    } else {
        m_contentEdit->clear();
        hide();
        m_hidden = true;
        emit dismissed();
    }
}

void QuickEntryDialog::onPinToDesktop()
{
    // Save first, then pin
    QString content = m_contentEdit->toPlainText().trimmed();
    if (content.isEmpty()) return;

    m_waitingForPin = true;
    onSave();
}

void QuickEntryDialog::onDiscard()
{
    m_contentEdit->clear();
    // Clear draft（仅清除本窗口草稿）
    QFile::remove(draftPath());
    hide();
    m_hidden = true;
    m_ghostState = false;
    setWindowOpacity(1.0);
    emit dismissed();
}

void QuickEntryDialog::setCascadeIndex(int index)
{
    m_cascadeIndex = index;
}

QString QuickEntryDialog::draftPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QString("/drafts/draft_%1.txt").arg(m_instanceId);
}

void QuickEntryDialog::dismissWithDraft()
{
    QString content = m_contentEdit->toPlainText();

    // 未保存内容以本窗口草稿保留，供下一个新窗口恢复
    if (!content.trimmed().isEmpty()) {
        QString draftDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/drafts";
        QDir().mkpath(draftDir);
        QFile draftFile(draftPath());
        if (draftFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            draftFile.write(content.toUtf8());
            draftFile.close();
            s_pendingDraftPath = draftPath();
        }
    }

    m_contentEdit->clear();
    hide();
    m_hidden = true;
    emit dismissed();
}

void QuickEntryDialog::restorePendingDraft()
{
    if (s_pendingDraftPath.isEmpty()) return;

    QFile draftFile(s_pendingDraftPath);
    if (draftFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString draft = QString::fromUtf8(draftFile.readAll()).trimmed();
        draftFile.close();
        if (!draft.isEmpty()) {
            m_contentEdit->setPlainText(draft);
        }
    }
    QFile::remove(s_pendingDraftPath);
    s_pendingDraftPath.clear();
}
