// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "stickynotecard.h"
#include "application/shorthandapplication.h"
#include "core/notemanager.h"
#include "core/tagmanager.h"
#include "ui/edgeautohide.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QMenu>
#include <QClipboard>
#include <QAction>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QDebug>
#include <DGuiApplicationHelper>

static const char *CARD_STYLE = R"(
    StickyNoteCard {
        border-radius: 12px;
    }
    QTextEdit#cardContent {
        border: none;
        border-radius: 8px;
        font-size: 13px;
        background: transparent;
        padding: 4px 8px;
        color: palette(windowText);
    }
    QTextEdit#cardContent:focus {
        border: 1px solid palette(highlight);
        background: palette(midlight);
    }
    QLabel#cardTime {
        font-size: 11px;
        color: palette(placeholderText);
        padding: 0 4px;
    }
    QPushButton#toolBtn {
        background: transparent;
        border: none;
        border-radius: 4px;
        font-size: 12px;
        padding: 2px 6px;
        color: palette(windowText);
        min-width: 20px;
    }
    QPushButton#toolBtn:hover {
        background: palette(highlight);
        color: palette(highlightedText);
    }
)";

static const QStringList COLOR_PALETTE = {
    "#409EFF", // 蓝
    "#67C23A", // 绿
    "#E6A23C", // 黄
    "#F56C6C", // 橙红
    "#B37FEB", // 紫
    "#F56C6C"  // 红
};

StickyNoteCard::StickyNoteCard(const NoteData &note, QWidget *parent)
    : QWidget(parent)
    , m_noteData(note)
{
    setObjectName("StickyNoteCard");
    setFixedSize(280, 160);
    setMinimumSize(220, 120);
    setMaximumSize(480, 360);
    setFocusPolicy(Qt::StrongFocus);

    // Shadow effect
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    auto *helper = DGuiApplicationHelper::instance();
    bool dark = helper && helper->themeType() == DGuiApplicationHelper::DarkType;
    shadow->setColor(dark ? QColor(0, 0, 0, 100) : QColor(0, 0, 0, 30));
    setGraphicsEffect(shadow);

    setStyleSheet(CARD_STYLE);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(800);
    connect(m_saveTimer, &QTimer::timeout, this, &StickyNoteCard::onTextChanged);

    m_edgeHide = new EdgeAutoHide(this, this);
    initUI();
    setupMenu();
}

StickyNoteCard::~StickyNoteCard() = default;

void StickyNoteCard::initUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(4);

    // Top bar: time + toolbar (hidden by default)
    QHBoxLayout *topRow = new QHBoxLayout();
    m_timeLabel = new QLabel(m_noteData.createdAt().toString("MM-dd HH:mm"), this);
    m_timeLabel->setObjectName("cardTime");
    topRow->addWidget(m_timeLabel);
    topRow->addStretch();

    // Toolbar (initially hidden)
    m_toolbar = new QWidget(this);
    m_toolbar->setObjectName("cardToolbar");
    QHBoxLayout *toolLayout = new QHBoxLayout(m_toolbar);
    toolLayout->setContentsMargins(0, 0, 0, 0);
    toolLayout->setSpacing(2);

    m_pinBtn = new QPushButton("📌", this);
    m_pinBtn->setObjectName("toolBtn");
    m_pinBtn->setFixedSize(24, 24);
    m_pinBtn->setToolTip(tr("置顶"));
    connect(m_pinBtn, &QPushButton::clicked, this, &StickyNoteCard::onPinToggled);
    toolLayout->addWidget(m_pinBtn);

    m_colorBtn = new QPushButton("🎨", this);
    m_colorBtn->setObjectName("toolBtn");
    m_colorBtn->setFixedSize(24, 24);
    m_colorBtn->setToolTip(tr("更改颜色"));
    connect(m_colorBtn, &QPushButton::clicked, this, &StickyNoteCard::onColorClicked);
    toolLayout->addWidget(m_colorBtn);

    m_deleteBtn = new QPushButton("🗑", this);
    m_deleteBtn->setObjectName("toolBtn");
    m_deleteBtn->setFixedSize(24, 24);
    m_deleteBtn->setToolTip(tr("删除"));
    connect(m_deleteBtn, &QPushButton::clicked, this, &StickyNoteCard::onDeleteClicked);
    toolLayout->addWidget(m_deleteBtn);

    m_toolbar->setLayout(toolLayout);
    m_toolbar->hide();

    topRow->addWidget(m_toolbar);
    layout->addLayout(topRow);

    // Content area
    m_contentEdit = new QTextEdit(this);
    m_contentEdit->setObjectName("cardContent");
    m_contentEdit->setPlaceholderText(tr("点击编辑..."));
    m_contentEdit->setAcceptRichText(false);
    m_contentEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_contentEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_contentEdit->setReadOnly(true);
    m_contentEdit->setFrameShape(QFrame::NoFrame);
    connect(m_contentEdit, &QTextEdit::textChanged, this, [this]() {
        m_isEditing = true;
        m_saveTimer->start();
    });
    layout->addWidget(m_contentEdit, 1);

    // Bottom status bar
    QHBoxLayout *bottomRow = new QHBoxLayout();
    QLabel *syncLabel = new QLabel(tr("已同步到笔记 · ") + m_noteData.modifiedAt().toString("yyyy-MM-dd"), this);
    syncLabel->setStyleSheet("font-size: 10px; color: palette(placeholderText);");
    bottomRow->addWidget(syncLabel);
    bottomRow->addStretch();

    // Resize handle
    m_resizeHandle = new QWidget(this);
    m_resizeHandle->setFixedSize(16, 16);
    m_resizeHandle->setCursor(Qt::SizeFDiagCursor);
    m_resizeHandle->setStyleSheet("background: transparent;");
    bottomRow->addWidget(m_resizeHandle);
    layout->addLayout(bottomRow);

    // Set content
    m_contentEdit->setPlainText(m_noteData.content);
}

void StickyNoteCard::setupMenu()
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);

        QAction *editAction = menu.addAction(tr("✏ 展开编辑"));
        connect(editAction, &QAction::triggered, this, [this]() {
            emit expandRequested(m_noteData.id);
        });

        QAction *copyAction = menu.addAction(tr("📋 复制"));
        connect(copyAction, &QAction::triggered, this, [this]() {
            QApplication::clipboard()->setText(m_contentEdit->toPlainText());
        });

        menu.addSeparator();

        QMenu *colorMenu = menu.addMenu(tr("🎨 更改颜色"));
        QStringList colorNames = {tr("蓝"), tr("绿"), tr("黄"), tr("橙"), tr("紫"), tr("红")};
        for (int i = 0; i < COLOR_PALETTE.size(); ++i) {
            QAction *ca = colorMenu->addAction(colorNames[i]);
            ca->setData(COLOR_PALETTE[i]);
            connect(ca, &QAction::triggered, this, [this, i]() {
                setCardColor(COLOR_PALETTE[i]);
            });
        }

        QAction *pinAction = menu.addAction(m_isPinned ? tr("📌 取消置顶") : tr("📌 置顶"));
        connect(pinAction, &QAction::triggered, this, &StickyNoteCard::onPinToggled);

        menu.addSeparator();

        QAction *removeAction = menu.addAction(tr("🗑 删除便签"));
        connect(removeAction, &QAction::triggered, this, &StickyNoteCard::onDeleteClicked);

        menu.exec(mapToGlobal(pos));
    });
}

void StickyNoteCard::setCardColor(const QString &color)
{
    m_cardColor = color;
    update();
}

void StickyNoteCard::updateStyleSheet()
{
    // Color is handled in paintEvent
    update();
}

bool StickyNoteCard::isEdgeDocked() const
{
    return m_edgeHide && m_edgeHide->isDocked();
}

void StickyNoteCard::fadeIn(int ms)
{
    show();
    raise();

    auto *effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(effect);
    auto *anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(ms);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void StickyNoteCard::fadeOut(int ms)
{
    auto *effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(effect);
    auto *anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(ms);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InCubic);
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        hide();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void StickyNoteCard::showToolbar(bool show)
{
    m_toolbar->setVisible(show);
}

void StickyNoteCard::onTextChanged()
{
    QString content = m_contentEdit->toPlainText().trimmed();
    if (content == m_noteData.content) return;

    m_noteData.content = content;
    m_isEditing = true;
    m_noteData.title = content.left(60).section('\n', 0, 0);
    emit noteUpdated(m_noteData.id, content);
}

void StickyNoteCard::onDeleteClicked()
{
    emit deleteRequested(m_noteData.id);
}

void StickyNoteCard::onColorClicked()
{
    // Cycle through colors
    int idx = COLOR_PALETTE.indexOf(m_cardColor);
    idx = (idx + 1) % COLOR_PALETTE.size();
    setCardColor(COLOR_PALETTE[idx]);
}

void StickyNoteCard::onPinToggled()
{
    m_isPinned = !m_isPinned;
    m_pinBtn->setText(m_isPinned ? "📍" : "📌");
    m_pinBtn->setToolTip(m_isPinned ? tr("取消置顶") : tr("置顶"));

    if (m_isPinned && window()) {
        window()->raise();
    }
}

void StickyNoteCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Check if click is on resize handle (bottom-right 16x16 area)
        QPoint localPos = event->pos();
        QRect handleRect(width() - 16, height() - 16, 16, 16);

        if (handleRect.contains(localPos)) {
            m_isResizing = true;
            m_dragStartPos = localPos;
            m_dragStartSize = size();
        } else if (localPos.y() < 30) {
            // Drag from top bar
            m_isDragging = true;
            m_dragStartPos = localPos;
            m_dragStartGlobal = event->globalPosition().toPoint();
            if (m_edgeHide) {
                m_edgeHide->startDrag(event->globalPosition().toPoint());
                grabMouse();
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void StickyNoteCard::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isResizing) {
        QPoint delta = event->pos() - m_dragStartPos;
        QSize newSize(m_dragStartSize.width() + delta.x(), m_dragStartSize.height() + delta.y());
        newSize = newSize.expandedTo(minimumSize()).boundedTo(maximumSize());
        resize(newSize);
    } else if (m_isDragging) {
        if (m_edgeHide) {
            m_edgeHide->dragTo(event->globalPosition().toPoint());
        } else {
            QPoint delta = event->globalPosition().toPoint() - m_dragStartGlobal;
            move(pos() + delta);
            m_dragStartGlobal = event->globalPosition().toPoint();
        }
    }
    QWidget::mouseMoveEvent(event);
}

void StickyNoteCard::mouseReleaseEvent(QMouseEvent *event)
{
    const bool hadDrag = m_isDragging;
    m_isDragging = false;
    m_isResizing = false;
    if (hadDrag) {
        releaseMouse();
    }
    // 先保存拖拽/缩放后的可见位置，再检测贴边隐藏
    emit geometryChanged(m_noteData.id);
    if (m_edgeHide) {
        m_edgeHide->endDrag();
    }
    QWidget::mouseReleaseEvent(event);
}

void StickyNoteCard::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit expandRequested(m_noteData.id);
    }
    QWidget::mouseDoubleClickEvent(event);
}

void StickyNoteCard::focusOutEvent(QFocusEvent *event)
{
    // Auto-save on focus loss — m_isEditing 跟踪用户在编辑状态
    if (m_isEditing) {
        onTextChanged();
        m_contentEdit->setReadOnly(true);
        m_isEditing = false;
    }
    QWidget::focusOutEvent(event);
}

void StickyNoteCard::resizeEvent(QResizeEvent *event)
{
    // Update resize handle position
    if (m_resizeHandle) {
        m_resizeHandle->move(width() - 16, height() - 16);
    }
    QWidget::resizeEvent(event);
}

void StickyNoteCard::enterEvent(QEnterEvent *event)
{
    showToolbar(true);
    // 鼠标悬停时允许编辑（但需要主动点击进入编辑状态）
    m_contentEdit->setReadOnly(false);
    QWidget::enterEvent(event);
}

void StickyNoteCard::leaveEvent(QEvent *event)
{
    showToolbar(false);
    // 鼠标离开时自动保存并切回只读
    if (m_contentEdit->hasFocus()) {
        m_contentEdit->clearFocus();
    }
    if (m_isEditing) {
        onTextChanged();
        m_contentEdit->setReadOnly(true);
        m_isEditing = false;
    }
    QWidget::leaveEvent(event);
}

void StickyNoteCard::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Parse card color
    QColor bgColor(m_cardColor);
    auto *helper = DGuiApplicationHelper::instance();
    bool dark = helper && helper->themeType() == DGuiApplicationHelper::DarkType;

    // Semi-transparent card background
    QColor fillColor = bgColor.lighter(dark ? 140 : 180);
    fillColor.setAlpha(dark ? 200 : 220);
    painter.setBrush(fillColor);

    // Border
    QPen borderPen(bgColor.darker(dark ? 120 : 110));
    borderPen.setWidthF(1.0);
    painter.setPen(borderPen);

    // Rounded rectangle
    QRectF rect(0.5, 0.5, width() - 1, height() - 1);
    painter.drawRoundedRect(rect, 12, 12);

    QWidget::paintEvent(event);
}
