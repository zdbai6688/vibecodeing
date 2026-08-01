// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "quickentrydialog.h"
#include "application/shorthandapplication.h"
#include "core/notemanager.h"
#include "core/tagmanager.h"
#include "ui/desktop/desktopmodemanager.h"

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

static const char *COMPACT_STYLE = R"(
    QWidget#compactContainer {
        background: palette(window);
        border: 1px solid palette(mid);
        border-radius: 6px;
    }
    QTextEdit#compactInput {
        border: none;
        font-size: 13px;
        background: transparent;
        padding: 4px;
    }
    QPushButton#compactBtn {
        background: transparent;
        border: none;
        border-radius: 6px;
        font-size: 12px;
        padding: 4px 8px;
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
        border-radius: 6px;
        font-size: 12px;
        padding: 4px 12px;
        font-weight: 600;
    }
    QPushButton#compactSave:hover {
        background: palette(dark);
    }
    QPushButton#pinBtn {
        background: transparent;
        border: 1px solid palette(mid);
        border-radius: 6px;
        font-size: 12px;
        padding: 4px 8px;
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

    initUI();
    initConnections();
}

void QuickEntryDialog::initUI()
{
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    m_modeStack = new QStackedWidget(this);

    initCompactUI();
    initFullUI();

    m_modeStack->setCurrentWidget(m_compactPage);
    outerLayout->addWidget(m_modeStack);
}

void QuickEntryDialog::initCompactUI()
{
    m_compactPage = new QWidget(this);
    m_compactPage->setObjectName("compactContainer");

    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 2);
    auto *helper = DGuiApplicationHelper::instance();
    bool isDark = helper && helper->themeType() == DGuiApplicationHelper::DarkType;
    shadow->setColor(isDark ? QColor(0, 0, 0, 120) : QColor(0, 0, 0, 40));
    m_compactPage->setGraphicsEffect(shadow);

    QVBoxLayout *layout = new QVBoxLayout(m_compactPage);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);

    // Top row: expand + continuous switch + close
    QHBoxLayout *topRow = new QHBoxLayout();
    m_expandBtn = new QPushButton(tr("□"), this);
    m_expandBtn->setObjectName("compactBtn");
    m_expandBtn->setToolTip(tr("展开"));
    m_expandBtn->setFixedSize(24, 24);
    topRow->addWidget(m_expandBtn);

    DLabel *contLabel = new DLabel(tr("连续"), this);
    contLabel->setStyleSheet("font-size: 11px; color: palette(placeholderText);");
    topRow->addWidget(contLabel);
    m_continuousSwitch = new DSwitchButton(this);
    m_continuousSwitch->setChecked(m_continuousAdd);
    m_continuousSwitch->setFixedSize(36, 20);
    m_continuousSwitch->setToolTip(tr("连续新增：保存后不关闭窗口"));
    topRow->addWidget(m_continuousSwitch);

    topRow->addStretch();
    m_compactBtn = new QPushButton(tr("✕"), this);
    m_compactBtn->setObjectName("compactBtn");
    m_compactBtn->setFixedSize(24, 24);
    m_compactBtn->setToolTip(tr("关闭"));
    topRow->addWidget(m_compactBtn);
    layout->addLayout(topRow);

    // Content
    m_compactEdit = new QTextEdit(this);
    m_compactEdit->setObjectName("compactInput");
    m_compactEdit->setPlaceholderText(tr("快速记录..."));
    m_compactEdit->setAcceptRichText(false);
    m_compactEdit->setFixedHeight(60);
    m_compactEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_compactEdit);

    // Bottom row: voice, screenshot, save, pin-to-desktop
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(4);
    QPushButton *voiceBtn = new QPushButton(tr("🎤"), this);
    voiceBtn->setObjectName("compactBtn");
    voiceBtn->setFixedSize(28, 28);
    voiceBtn->setToolTip(tr("语音"));
    QPushButton *ssBtn = new QPushButton(tr("📷"), this);
    ssBtn->setObjectName("compactBtn");
    ssBtn->setFixedSize(28, 28);
    ssBtn->setToolTip(tr("截图"));
    bottomRow->addWidget(voiceBtn);
    bottomRow->addWidget(ssBtn);
    bottomRow->addStretch();

    m_pinToDesktopBtn = new QPushButton(tr("📌 贴到桌面"), this);
    m_pinToDesktopBtn->setObjectName("pinBtn");
    m_pinToDesktopBtn->setFixedHeight(26);
    m_pinToDesktopBtn->setToolTip(tr("保存并贴到桌面便签"));
    bottomRow->addWidget(m_pinToDesktopBtn);

    QPushButton *saveBtn = new QPushButton(tr("保存"), this);
    saveBtn->setObjectName("compactSave");
    saveBtn->setFixedHeight(26);
    bottomRow->addWidget(saveBtn);
    layout->addLayout(bottomRow);

    connect(voiceBtn, &QPushButton::clicked, this, &QuickEntryDialog::onVoiceInput);
    connect(ssBtn, &QPushButton::clicked, this, &QuickEntryDialog::onScreenshot);
    connect(saveBtn, &QPushButton::clicked, this, &QuickEntryDialog::onSave);
    connect(m_pinToDesktopBtn, &QPushButton::clicked, this, &QuickEntryDialog::onPinToDesktop);
    connect(m_expandBtn, &QPushButton::clicked, this, [this]() {
        m_compactMode = false;
        m_modeStack->setCurrentWidget(m_fullPage);
        m_contentEdit->setPlainText(m_compactEdit->toPlainText());
        m_contentEdit->setFocus();
        setFixedSize(520, 260);
        centerOnScreen();
    });
    connect(m_compactBtn, &QPushButton::clicked, this, [this]() {
        if (!m_compactEdit->toPlainText().trimmed().isEmpty()) {
            // Draft: save to draft file only
            QString draftDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/drafts";
            QDir().mkpath(draftDir);
            QFile draftFile(draftDir + "/draft.txt");
            if (draftFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                draftFile.write(m_compactEdit->toPlainText().toUtf8());
                draftFile.close();
            }
        }
        m_compactEdit->clear();
        hide();
        m_hidden = true;
    });
    connect(m_compactEdit, &QTextEdit::textChanged, this, [this]() {
        if (m_compactEdit->toPlainText().contains('\n')) {
            QString text = m_compactEdit->toPlainText();
            m_contentEdit->setPlainText(text);
            m_compactMode = false;
            m_modeStack->setCurrentWidget(m_fullPage);
            setFixedSize(520, 260);
            centerOnScreen();
            m_contentEdit->moveCursor(QTextCursor::End);
            m_contentEdit->setFocus();
        }
    });

    connect(m_continuousSwitch, &DSwitchButton::toggled, this, [this](bool checked) {
        m_continuousAdd = checked;
        QSettings().setValue("desktop/continuous_add", checked);
    });

    m_modeStack->addWidget(m_compactPage);
}

void QuickEntryDialog::initFullUI()
{
    m_fullPage = new QWidget(this);
    m_fullPage->setObjectName("compactContainer");
    QVBoxLayout *layout = new QVBoxLayout(m_fullPage);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);

    QHBoxLayout *topRow = new QHBoxLayout();
    QPushButton *compactToggleBtn = new QPushButton(tr("−"), this);
    compactToggleBtn->setObjectName("compactBtn");
    compactToggleBtn->setToolTip(tr("收缩"));
    compactToggleBtn->setFixedSize(24, 24);
    topRow->addWidget(compactToggleBtn);

    DLabel *contLabel = new DLabel(tr("连续"), this);
    contLabel->setStyleSheet("font-size: 11px; color: palette(placeholderText);");
    topRow->addWidget(contLabel);
    DSwitchButton *contSwitch = new DSwitchButton(this);
    contSwitch->setChecked(m_continuousAdd);
    contSwitch->setFixedSize(36, 20);
    contSwitch->setToolTip(tr("连续新增：保存后不关闭窗口"));
    topRow->addWidget(contSwitch);
    connect(contSwitch, &DSwitchButton::toggled, this, [this](bool checked) {
        m_continuousAdd = checked;
        QSettings().setValue("desktop/continuous_add", checked);
    });

    topRow->addStretch();
    QPushButton *cancelBtn = new QPushButton(tr("✕"), this);
    cancelBtn->setObjectName("compactBtn");
    cancelBtn->setFixedSize(24, 24);
    cancelBtn->setToolTip(tr("关闭"));
    topRow->addWidget(cancelBtn);
    layout->addLayout(topRow);

    m_contentEdit = new QTextEdit(this);
    m_contentEdit->setObjectName("compactInput");
    m_contentEdit->setPlaceholderText(tr("记录笔记内容...\n\n支持 #标签 和 !优先级"));
    m_contentEdit->setAcceptRichText(false);
    m_contentEdit->setFixedHeight(120);
    layout->addWidget(m_contentEdit, 1);

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(4);
    m_hintLabel = new DLabel(tr("按 Enter 保存 | Ctrl+Enter 换行"), this);
    m_hintLabel->setStyleSheet("font-size: 10px; color: palette(placeholderText);");
    bottomRow->addWidget(m_hintLabel);
    bottomRow->addStretch();

    QPushButton *pinBtn = new QPushButton(tr("📌 贴到桌面"), this);
    pinBtn->setObjectName("pinBtn");
    pinBtn->setFixedHeight(26);
    pinBtn->setToolTip(tr("保存并贴到桌面便签"));
    bottomRow->addWidget(pinBtn);
    connect(pinBtn, &QPushButton::clicked, this, &QuickEntryDialog::onPinToDesktop);

    QPushButton *fullSaveBtn = new QPushButton(tr("保存"), this);
    fullSaveBtn->setObjectName("compactSave");
    fullSaveBtn->setFixedHeight(26);
    bottomRow->addWidget(fullSaveBtn);
    layout->addLayout(bottomRow);

    connect(compactToggleBtn, &QPushButton::clicked, this, [this]() {
        m_compactEdit->setPlainText(m_contentEdit->toPlainText());
        m_compactMode = true;
        m_modeStack->setCurrentWidget(m_compactPage);
        setFixedSize(280, 160);
        centerOnScreen();
        m_compactEdit->setFocus();
    });
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        if (!m_contentEdit->toPlainText().trimmed().isEmpty()) {
            // Draft save
            QString draftDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/drafts";
            QDir().mkpath(draftDir);
            QFile draftFile(draftDir + "/draft.txt");
            if (draftFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                draftFile.write(m_contentEdit->toPlainText().toUtf8());
                draftFile.close();
            }
        }
        m_contentEdit->clear();
        hide();
        m_hidden = true;
    });
    connect(fullSaveBtn, &QPushButton::clicked, this, &QuickEntryDialog::onSave);

    m_modeStack->addWidget(m_fullPage);
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
    // Check for saved draft
    QString draftPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/drafts/draft.txt";
    QFile draftFile(draftPath);
    if (draftFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString draft = QString::fromUtf8(draftFile.readAll()).trimmed();
        draftFile.close();
        if (!draft.isEmpty()) {
            m_compactEdit->setPlainText(draft);
        }
        // Clear draft after restore
        QFile::remove(draftPath);
    }

    if (m_compactMode) {
        m_modeStack->setCurrentWidget(m_compactPage);
        setFixedSize(280, 160);
    } else {
        m_modeStack->setCurrentWidget(m_fullPage);
        setFixedSize(520, 260);
    }
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
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    QRect screenGeo = screen->availableGeometry();
    move(screenGeo.center() - rect().center());
}

void QuickEntryDialog::setFocus()
{
    if (m_hidden) {
        showCompact();
    }
    QWidget::setFocus();
    if (m_compactMode) {
        m_compactEdit->setFocus();
    } else {
        m_contentEdit->setFocus();
    }
    m_ghostState = false;
    setWindowOpacity(1.0);
}

void QuickEntryDialog::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    centerOnScreen();
    m_contentEdit->setFocus();
}

void QuickEntryDialog::toggleCompactMode()
{
    m_compactMode = !m_compactMode;
    if (m_compactMode) {
        m_compactEdit->setPlainText(m_contentEdit->toPlainText());
        m_modeStack->setCurrentWidget(m_compactPage);
        setFixedSize(280, 160);
    } else {
        m_contentEdit->setPlainText(m_compactEdit->toPlainText());
        m_modeStack->setCurrentWidget(m_fullPage);
        setFixedSize(520, 260);
    }
    centerOnScreen();
}

void QuickEntryDialog::enterGhostState()
{
    if (m_ghostState || m_hidden) return;
    m_ghostState = true;

    // Save draft
    QString content;
    if (m_compactMode) content = m_compactEdit->toPlainText();
    else content = m_contentEdit->toPlainText();

    if (!content.trimmed().isEmpty()) {
        QString draftDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/drafts";
        QDir().mkpath(draftDir);
        QFile draftFile(draftDir + "/draft.txt");
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

void QuickEntryDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (event->modifiers() & Qt::ControlModifier) {
            if (m_compactMode) {
                m_compactEdit->insertPlainText("\n");
            } else {
                m_contentEdit->insertPlainText("\n");
            }
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
    QString content;
    if (m_compactMode) content = m_compactEdit->toPlainText();
    else content = m_contentEdit->toPlainText();

    if (!content.trimmed().isEmpty() && !m_hidden) {
        enterGhostState();
    }
    QWidget::focusOutEvent(event);
}

void QuickEntryDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange) {
        if (!isActiveWindow() && !m_hidden) {
            QString content;
            if (m_compactMode) content = m_compactEdit->toPlainText();
            else content = m_contentEdit->toPlainText();
            if (!content.trimmed().isEmpty()) {
                enterGhostState();
            }
        }
    }
    QWidget::changeEvent(event);
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
        if (m_compactMode) {
            m_compactEdit->insertPlainText(text);
        } else {
            m_contentEdit->insertPlainText(text);
        }
    }
}

void QuickEntryDialog::onScreenshot()
{
    QString ts = QString::number(QDateTime::currentSecsSinceEpoch());
    QString text = tr("[截图: %1.png]").arg(ts);
    if (m_compactMode) {
        m_compactEdit->insertPlainText(text);
    } else {
        m_contentEdit->insertPlainText(text);
    }
}

void QuickEntryDialog::onSave()
{
    QString content;
    if (m_compactMode) {
        content = m_compactEdit->toPlainText().trimmed();
    } else {
        content = m_contentEdit->toPlainText().trimmed();
    }

    if (content.isEmpty()) {
        hide();
        m_hidden = true;
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
    }

    m_lastSavedNoteId = id;

    // Handle pin-to-desktop if waiting
    if (m_waitingForPin && id > 0) {
        app->desktopModeManager()->pinNoteToDesktop(id);
        if (!app->desktopModeManager()->isDesktopMode()) {
            app->desktopModeManager()->enterDesktopMode();
        }
        m_waitingForPin = false;
    }

    if (m_continuousAdd) {
        // Clear but don't close
        if (m_compactMode) m_compactEdit->clear();
        else m_contentEdit->clear();
        if (m_compactMode) m_compactEdit->setFocus();
        else m_contentEdit->setFocus();
    } else {
        if (m_compactMode) m_compactEdit->clear();
        else m_contentEdit->clear();
        hide();
        m_hidden = true;
    }
}

void QuickEntryDialog::onPinToDesktop()
{
    // Save first, then pin
    QString content;
    if (m_compactMode) content = m_compactEdit->toPlainText().trimmed();
    else content = m_contentEdit->toPlainText().trimmed();

    if (content.isEmpty()) return;

    m_waitingForPin = true;
    onSave();
}

void QuickEntryDialog::onDiscard()
{
    m_compactEdit->clear();
    m_contentEdit->clear();
    // Clear draft
    QString draftPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/drafts/draft.txt";
    QFile::remove(draftPath);
    hide();
    m_hidden = true;
    m_ghostState = false;
    setWindowOpacity(1.0);
}
