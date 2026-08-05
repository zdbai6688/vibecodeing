// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sidebarwidget.h"
#include "application/shorthandapplication.h"
#include "core/notemanager.h"
#include "core/todomanager.h"
#include "core/tagmanager.h"

#include <DLabel>
#include <DFontSizeManager>
#include <DGuiApplicationHelper>
#include <DPalette>
#include <QDebug>
#include <QAction>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QDateTime>

SidebarWidget::SidebarWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("SidebarWidget");
    setFixedWidth(EXPANDED_WIDTH);
    m_currentWidth = EXPANDED_WIDTH;

    m_badgeNotes = nullptr;
    m_badgeTodos = nullptr;

    initUI();
    refreshStyleSheet();

    // 监听主题变化
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged,
            this, &SidebarWidget::refreshStyleSheet);

    auto *app = ShorthandApplication::instance();
    connect(app->tagManager(), &TagManager::dataChanged, this, &SidebarWidget::updateTagList);
    connect(app->noteManager(), &NoteManager::dataChanged, this, [this, app]() {
        updateBadge(app->noteManager()->noteCount(), app->todoManager()->pendingCount());
    });
    connect(app->todoManager(), &TodoManager::dataChanged, this, [this, app]() {
        updateBadge(app->noteManager()->noteCount(), app->todoManager()->pendingCount());
    });

    // 启动时读取真实数量
    updateBadge(app->noteManager()->noteCount(), app->todoManager()->pendingCount());
}

void SidebarWidget::initUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // ===== Header 区域 =====
    m_headerWidget = new QWidget(this);
    m_headerWidget->setObjectName("sidebarHeader");
    QHBoxLayout *headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(12, 8, 8, 8);
    headerLayout->setSpacing(6);

    m_logoIcon = new QLabel(QString::fromUtf8("\xF0\x9F\x93\x9D"), this);
    m_logoIcon->setObjectName("logoIcon");
    m_logoIcon->setFixedSize(28, 28);
    m_logoIcon->setAlignment(Qt::AlignCenter);

    m_logoText = new QLabel(tr("UOS速记"), this);
    m_logoText->setObjectName("logoText");

    m_collapseBtn = new QPushButton(this);
    m_collapseBtn->setObjectName("collapseBtn");
    m_collapseBtn->setFixedSize(24, 24);
    m_collapseBtn->setCursor(Qt::PointingHandCursor);
    m_collapseBtn->setToolTip(tr("折叠侧栏"));
    m_collapseBtn->setText(QString::fromUtf8("\xE2\x97\x80")); // ◀
    connect(m_collapseBtn, &QPushButton::clicked, this, &SidebarWidget::toggleCollapse);

    headerLayout->addWidget(m_logoIcon);
    headerLayout->addWidget(m_logoText, 1);
    headerLayout->addWidget(m_collapseBtn);
    m_mainLayout->addWidget(m_headerWidget);

    // ===== 可滚动内容区域 =====
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setObjectName("sidebarScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget *scrollContent = new QWidget(scroll);
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(6, 8, 6, 8);
    scrollLayout->setSpacing(2);

    // ===== 第一组：核心功能 =====
    m_coreGroup = new QWidget(this);
    m_coreGroup->setObjectName("coreGroup");
    QVBoxLayout *coreLayout = new QVBoxLayout(m_coreGroup);
    coreLayout->setContentsMargins(0, 0, 0, 0);
    coreLayout->setSpacing(1);

    // 核心功能区段标题
    DLabel *sectionCore = new DLabel(tr("核心功能"), this);
    sectionCore->setObjectName("sectionCore");
    sectionCore->setFixedHeight(24);
    sectionCore->setContentsMargins(12, 4, 12, 2);
    QFont f = sectionCore->font();
    f.setPointSize(9);
    f.setBold(true);
    sectionCore->setFont(f);
    coreLayout->addWidget(sectionCore);

    // 笔记按钮
    m_btnNotes = new QPushButton(this);
    m_btnNotes->setObjectName("navBtn");
    m_btnNotes->setCheckable(true);
    m_btnNotes->setCursor(Qt::PointingHandCursor);
    m_btnNotes->setFixedHeight(34);
    QHBoxLayout *notesLayout = new QHBoxLayout(m_btnNotes);
    notesLayout->setContentsMargins(10, 0, 6, 0);
    notesLayout->setSpacing(6);
    QLabel *notesIcon = new QLabel(QString::fromUtf8("\xF0\x9F\x93\x9D"), m_btnNotes);
    notesIcon->setObjectName("navIcon");
    notesIcon->setFixedSize(20, 20);
    notesIcon->setAlignment(Qt::AlignCenter);
    QLabel *notesText = new QLabel(tr("全部笔记"), m_btnNotes);
    notesText->setObjectName("navText");
    m_badgeNotes = new QLabel("", m_btnNotes);
    m_badgeNotes->setObjectName("navBadge");
    m_badgeNotes->setFixedSize(22, 20);
    m_badgeNotes->setAlignment(Qt::AlignCenter);
    m_badgeNotes->hide();
    notesLayout->addWidget(notesIcon);
    notesLayout->addWidget(notesText, 1);
    notesLayout->addWidget(m_badgeNotes);
    coreLayout->addWidget(m_btnNotes);

    // 待办按钮
    m_btnTodos = new QPushButton(this);
    m_btnTodos->setObjectName("navBtn");
    m_btnTodos->setCheckable(true);
    m_btnTodos->setCursor(Qt::PointingHandCursor);
    m_btnTodos->setFixedHeight(34);
    QHBoxLayout *todosLayout = new QHBoxLayout(m_btnTodos);
    todosLayout->setContentsMargins(10, 0, 6, 0);
    todosLayout->setSpacing(6);
    QLabel *todosIcon = new QLabel(QString::fromUtf8("\xE2\x9C\x85"), m_btnTodos);
    todosIcon->setObjectName("navIcon");
    todosIcon->setFixedSize(20, 20);
    todosIcon->setAlignment(Qt::AlignCenter);
    QLabel *todosText = new QLabel(tr("待办"), m_btnTodos);
    todosText->setObjectName("navText");
    m_badgeTodos = new QLabel("", m_btnTodos);
    m_badgeTodos->setObjectName("navBadge");
    m_badgeTodos->setFixedSize(22, 20);
    m_badgeTodos->setAlignment(Qt::AlignCenter);
    m_badgeTodos->hide();
    todosLayout->addWidget(todosIcon);
    todosLayout->addWidget(todosText, 1);
    todosLayout->addWidget(m_badgeTodos);
    coreLayout->addWidget(m_btnTodos);

    // 会议按钮
    m_btnMeetings = new QPushButton(this);
    m_btnMeetings->setObjectName("navBtn");
    m_btnMeetings->setCheckable(true);
    m_btnMeetings->setCursor(Qt::PointingHandCursor);
    m_btnMeetings->setFixedHeight(34);
    QHBoxLayout *meetingsLayout = new QHBoxLayout(m_btnMeetings);
    meetingsLayout->setContentsMargins(10, 0, 10, 0);
    meetingsLayout->setSpacing(6);
    QLabel *meetingsIcon = new QLabel(QString::fromUtf8("\xF0\x9F\x8E\xA4"), m_btnMeetings);
    meetingsIcon->setObjectName("navIcon");
    meetingsIcon->setFixedSize(20, 20);
    meetingsIcon->setAlignment(Qt::AlignCenter);
    QLabel *meetingsText = new QLabel(tr("会议"), m_btnMeetings);
    meetingsText->setObjectName("navText");
    meetingsLayout->addWidget(meetingsIcon);
    meetingsLayout->addWidget(meetingsText, 1);
    coreLayout->addWidget(m_btnMeetings);

    scrollLayout->addWidget(m_coreGroup);

    // ===== 第二组：标签筛选 =====
    m_tagGroup = new QWidget(this);
    m_tagGroup->setObjectName("tagGroup");
    QVBoxLayout *tagGroupLayout = new QVBoxLayout(m_tagGroup);
    tagGroupLayout->setContentsMargins(0, 0, 0, 0);
    tagGroupLayout->setSpacing(1);

    DLabel *sectionTags = new DLabel(tr("标签筛选"), this);
    sectionTags->setObjectName("sectionTags");
    sectionTags->setFixedHeight(24);
    sectionTags->setContentsMargins(12, 4, 12, 2);
    QFont ft = sectionTags->font();
    ft.setPointSize(9);
    ft.setBold(true);
    sectionTags->setFont(ft);
    tagGroupLayout->addWidget(sectionTags);

    m_tagList = new QListWidget(m_tagGroup);
    m_tagList->setObjectName("tagList");
    m_tagList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tagList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tagList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tagList->setFrameShape(QFrame::NoFrame);
    connect(m_tagList, &QListWidget::customContextMenuRequested,
            this, &SidebarWidget::showTagContextMenu);
    connect(m_tagList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        QString tag = item->data(Qt::UserRole).toString();
        emit tagClicked(tag);
    });
    tagGroupLayout->addWidget(m_tagList);
    scrollLayout->addWidget(m_tagGroup);

    // ===== 第三组：归档 =====
    m_archiveGroup = new QWidget(this);
    m_archiveGroup->setObjectName("archiveGroup");
    QVBoxLayout *archiveLayout = new QVBoxLayout(m_archiveGroup);
    archiveLayout->setContentsMargins(0, 0, 0, 0);
    archiveLayout->setSpacing(1);

    DLabel *sectionArchive = new DLabel(tr("归档"), this);
    sectionArchive->setObjectName("sectionArchive");
    sectionArchive->setFixedHeight(24);
    sectionArchive->setContentsMargins(12, 4, 12, 2);
    QFont fa = sectionArchive->font();
    fa.setPointSize(9);
    fa.setBold(true);
    sectionArchive->setFont(fa);
    archiveLayout->addWidget(sectionArchive);

    // 已完成待办按钮（归档组第一个）
    m_btnCompleted = new QPushButton(this);
    m_btnCompleted->setObjectName("navBtn");
    m_btnCompleted->setCheckable(true);
    m_btnCompleted->setCursor(Qt::PointingHandCursor);
    m_btnCompleted->setFixedHeight(34);
    QHBoxLayout *completedLayout = new QHBoxLayout(m_btnCompleted);
    completedLayout->setContentsMargins(10, 0, 10, 0);
    completedLayout->setSpacing(6);
    QLabel *completedIcon = new QLabel(QString::fromUtf8("\xE2\x98\x91\xEF\xB8\x8F"), m_btnCompleted);
    completedIcon->setObjectName("navIcon");
    completedIcon->setFixedSize(20, 20);
    completedIcon->setAlignment(Qt::AlignCenter);
    QLabel *completedText = new QLabel(tr("已完成待办"), m_btnCompleted);
    completedText->setObjectName("navText");
    completedLayout->addWidget(completedIcon);
    completedLayout->addWidget(completedText, 1);
    archiveLayout->addWidget(m_btnCompleted);


    // 周报按钮
    m_btnWeekly = new QPushButton(this);
    m_btnWeekly->setObjectName("navBtn");
    m_btnWeekly->setCheckable(true);
    m_btnWeekly->setCursor(Qt::PointingHandCursor);
    m_btnWeekly->setFixedHeight(34);
    QHBoxLayout *weeklyLayout = new QHBoxLayout(m_btnWeekly);
    weeklyLayout->setContentsMargins(10, 0, 10, 0);
    weeklyLayout->setSpacing(6);
    QLabel *weeklyIcon = new QLabel(QString::fromUtf8("\xF0\x9F\x93\x8A"), m_btnWeekly);
    weeklyIcon->setObjectName("navIcon");
    weeklyIcon->setFixedSize(20, 20);
    weeklyIcon->setAlignment(Qt::AlignCenter);
    QLabel *weeklyText = new QLabel(tr("周报"), m_btnWeekly);
    weeklyText->setObjectName("navText");
    weeklyLayout->addWidget(weeklyIcon);
    weeklyLayout->addWidget(weeklyText, 1);
    archiveLayout->addWidget(m_btnWeekly);

    m_btnTrash = new QPushButton(this);
    m_btnTrash->setObjectName("navBtn");
    m_btnTrash->setCheckable(true);
    m_btnTrash->setCursor(Qt::PointingHandCursor);
    m_btnTrash->setFixedHeight(34);
    QHBoxLayout *trashLayout = new QHBoxLayout(m_btnTrash);
    trashLayout->setContentsMargins(10, 0, 10, 0);
    trashLayout->setSpacing(6);
    QLabel *trashIcon = new QLabel(QString::fromUtf8("\xF0\x9F\x97\x91"), m_btnTrash);
    trashIcon->setObjectName("navIcon");
    trashIcon->setFixedSize(20, 20);
    trashIcon->setAlignment(Qt::AlignCenter);
    QLabel *trashText = new QLabel(tr("最近删除"), m_btnTrash);
    trashText->setObjectName("navText");
    trashLayout->addWidget(trashIcon);
    trashLayout->addWidget(trashText, 1);
    archiveLayout->addWidget(m_btnTrash);

    scrollLayout->addWidget(m_archiveGroup);

    // 弹性空间
    scrollLayout->addStretch(1);

    scroll->setWidget(scrollContent);
    m_mainLayout->addWidget(scroll, 1);

    // ===== 底部设置按钮 =====
    QWidget *bottomWidget = new QWidget(this);
    bottomWidget->setObjectName("sidebarBottom");
    QVBoxLayout *bottomLayout = new QVBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(6, 4, 6, 8);
    bottomLayout->setSpacing(2);

    m_btnSettings = new QPushButton(this);
    m_btnSettings->setObjectName("navBtn");
    m_btnSettings->setCheckable(true);
    m_btnSettings->setCursor(Qt::PointingHandCursor);
    m_btnSettings->setFixedHeight(34);
    QHBoxLayout *settingsLayout = new QHBoxLayout(m_btnSettings);
    settingsLayout->setContentsMargins(10, 0, 10, 0);
    settingsLayout->setSpacing(6);
    QLabel *settingsIcon = new QLabel(QString::fromUtf8("\xE2\x9A\x99"), m_btnSettings);
    settingsIcon->setObjectName("navIcon");
    settingsIcon->setFixedSize(20, 20);
    settingsIcon->setAlignment(Qt::AlignCenter);
    QLabel *settingsText = new QLabel(tr("设置"), m_btnSettings);
    settingsText->setObjectName("navText");
    settingsLayout->addWidget(settingsIcon);
    settingsLayout->addWidget(settingsText, 1);
    bottomLayout->addWidget(m_btnSettings);
    m_mainLayout->addWidget(bottomWidget);

    // ===== 信号连接 =====
    connect(m_btnNotes, &QPushButton::clicked, this, &SidebarWidget::notesClicked);
    connect(m_btnTodos, &QPushButton::clicked, this, &SidebarWidget::todosClicked);
    connect(m_btnMeetings, &QPushButton::clicked, this, &SidebarWidget::meetingsClicked);
    connect(m_btnWeekly, &QPushButton::clicked, this, &SidebarWidget::weeklyClicked);
    connect(m_btnCompleted, &QPushButton::clicked, this, &SidebarWidget::completedTodosClicked);
    connect(m_btnTrash, &QPushButton::clicked, this, &SidebarWidget::trashClicked);
    connect(m_btnSettings, &QPushButton::clicked, this, &SidebarWidget::settingsClicked);

    // 初始化标签列表
    updateTagList();
}

void SidebarWidget::setActiveSection(int index)
{
    m_activeSection = index;
    m_btnNotes->setChecked(index == 0);
    m_btnTodos->setChecked(index == 1);
    m_btnMeetings->setChecked(index == 2);
    m_btnWeekly->setChecked(index == 5);
    m_btnCompleted->setChecked(index == 4);
    m_btnTrash->setChecked(index == 3);
    m_btnSettings->setChecked(index == 6);

    // 切换导航时取消标签选中，保证导航与标签视图互斥
    if (m_tagList) {
        m_tagList->clearSelection();
        m_tagList->setCurrentItem(nullptr);
    }
}

void SidebarWidget::activateTag(const QString &tag)
{
    // 标签视图下不选中任何导航按钮（与导航视图互斥）
    m_btnNotes->setChecked(false);
    m_btnTodos->setChecked(false);
    m_btnMeetings->setChecked(false);
    m_btnWeekly->setChecked(false);
    m_btnCompleted->setChecked(false);
    m_btnTrash->setChecked(false);
    m_btnSettings->setChecked(false);

    // 仅高亮当前标签
    if (!m_tagList) return;
    m_tagList->clearSelection();
    for (int i = 0; i < m_tagList->count(); ++i) {
        QListWidgetItem *item = m_tagList->item(i);
        if (item->data(Qt::UserRole).toString() == tag) {
            item->setSelected(true);
            m_tagList->setCurrentItem(item);
            break;
        }
    }
}

void SidebarWidget::updateBadge(int notes, int todos)
{
    // PRD §3.3: 角标规则
    if (notes > 0 && m_badgeNotes) {
        m_badgeNotes->setText(formatBadgeText(notes));
        m_badgeNotes->show();
    } else if (m_badgeNotes) {
        m_badgeNotes->hide();
    }

    if (todos > 0 && m_badgeTodos) {
        m_badgeTodos->setText(formatBadgeText(todos));
        m_badgeTodos->show();
    } else if (m_badgeTodos) {
        m_badgeTodos->hide();
    }
}

QString SidebarWidget::formatBadgeText(int count) const
{
    // PRD §3.3: ≤99 显示数字，>99 显示 "99+"
    if (count <= 0) return "";
    if (count > 99) return "99+";
    return QString::number(count);
}

void SidebarWidget::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed) return;
    m_collapsed = collapsed;

    int targetWidth = m_collapsed ? COLLAPSED_WIDTH : EXPANDED_WIDTH;

    QPropertyAnimation *anim = new QPropertyAnimation(this, "sidebarWidth", this);
    anim->setDuration(180);
    anim->setStartValue(m_currentWidth);
    anim->setEndValue(targetWidth);
    anim->setEasingCurve(QEasingCurve::InOutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    m_currentWidth = targetWidth;
    updateItemVisibility();

    m_collapseBtn->setText(m_collapsed
        ? QString::fromUtf8("\xE2\x96\xB6")   // ▶
        : QString::fromUtf8("\xE2\x97\x80")); // ◀
    m_collapseBtn->setToolTip(m_collapsed ? tr("展开侧栏") : tr("折叠侧栏"));

    emit collapseChanged(m_collapsed);
}

void SidebarWidget::toggleCollapse()
{
    setCollapsed(!m_collapsed);
}

void SidebarWidget::setSidebarWidth(int w)
{
    m_currentWidth = w;
    setFixedWidth(w);
    updateGeometry();
}

void SidebarWidget::updateItemVisibility()
{
    bool show = !m_collapsed;

    // 折叠时隐藏文字标签、角标和分组标题
    QList<QLabel*> textLabels = findChildren<QLabel*>("navText");
    for (auto *label : textLabels) {
        label->setVisible(show);
    }

    // 折叠标签列表
    if (m_tagList) {
        m_tagList->setVisible(show);
    }

    // 分组标题
    QList<DLabel*> sectionLabels = findChildren<DLabel*>();
    for (auto *label : sectionLabels) {
        QString on = label->objectName();
        if (on == "sectionCore" || on == "sectionTags" || on == "sectionArchive") {
            label->setVisible(show);
        }
    }

    // Logo 文字
    if (m_logoText) m_logoText->setVisible(show);
    if (m_collapseBtn) m_collapseBtn->setVisible(show);
}

// ─── 样式 ──────────────────────────────────────────────────────

void SidebarWidget::refreshStyleSheet()
{
    auto *helper = DGuiApplicationHelper::instance();
    bool dark = (helper->themeType() == DGuiApplicationHelper::DarkType);

    DPalette pal = helper->applicationPalette();
    QColor accent = pal.color(DPalette::Highlight);

    // 扁平背景 — 视觉规范 §1.4 bg-sidebar（全部取自 DPalette，自动适配深色主题）
    QColor bgSidebar = pal.color(QPalette::AlternateBase);
    QColor textPrimary = pal.color(QPalette::WindowText);
    QColor textSecondary = pal.color(DPalette::TextTips);
    QColor borderColor = pal.color(DPalette::FrameBorder);
    QColor badgeBg = accent;
    QColor badgeText = pal.color(QPalette::HighlightedText);

    QString accentStr = accent.name();
    QString accentSoftStr = QString("rgba(%1,%2,%3,%4)")
        .arg(accent.red()).arg(accent.green()).arg(accent.blue())
        .arg(dark ? 0.12 : 0.10);
    QString bgSidebarStr = bgSidebar.name();
    QString textPrimaryStr = textPrimary.name();
    QString textSecondaryStr = textSecondary.name();
    QString borderStr = borderColor.name();
    QString badgeBgStr = badgeBg.name();
    QString badgeTextStr = badgeText.name();

    setStyleSheet(QString(R"(
        SidebarWidget#SidebarWidget {
            background: %1;
            border-right: 1px solid %2;
        }
        #sidebarHeader {
            border-bottom: 1px solid %2;
        }
        #logoIcon {
            font-size: 18px;
        }
        #logoText {
            font-size: 14px;
            font-weight: 600;
            color: %3;
        }
        #collapseBtn {
            background: transparent;
            border: none;
            color: %4;
            font-size: 11px;
            border-radius: 4px;
        }
        #collapseBtn:hover {
            background: rgba(128,128,128,0.15);
        }
        DLabel#sectionCore,
        DLabel#sectionTags,
        DLabel#sectionArchive {
            color: %4;
            font-size: 11px;
            font-weight: 600;
            padding: 4px 12px 2px 12px;
            background: transparent;
            border: none;
        }
        QPushButton#navBtn {
            background: transparent;
            border: none;
            border-radius: 6px;
            text-align: left;
        }
        QPushButton#navBtn:hover {
            background: rgba(128,128,128,0.10);
        }
        QPushButton#navBtn:checked {
            background: %6;
        }
        QLabel#navIcon {
            font-size: 16px;
            color: %3;
        }
        QPushButton#navBtn:checked QLabel#navIcon {
            color: %5;
        }
        QLabel#navText {
            font-size: 13px;
            color: %3;
        }
        QPushButton#navBtn:checked QLabel#navText {
            color: %5;
            font-weight: 600;
        }
        QLabel#navBadge {
            background: %7;
            color: %8;
            font-size: 11px;
            font-weight: 600;
            border-radius: 10px;
            padding: 0 4px;
            min-width: 18px;
        }
        QListWidget#tagList {
            background: transparent;
            border: none;
            padding: 0 4px;
        }
        QListWidget#tagList::item {
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 13px;
            color: %3;
        }
        QListWidget#tagList::item:hover {
            background: rgba(128,128,128,0.10);
        }
        QListWidget#tagList::item:selected {
            background: %6;
            color: %5;
            font-weight: 500;
        }
        QScrollArea#sidebarScroll {
            background: transparent;
            border: none;
        }
        QScrollArea#sidebarScroll > QWidget > QWidget {
            background: transparent;
        }
        #sidebarBottom {
            border-top: 1px solid %2;
            background: transparent;
        }
    )").arg(bgSidebarStr, borderStr, textPrimaryStr, textSecondaryStr,
            accentStr, accentSoftStr, badgeBgStr, badgeTextStr));
}

// ─── 标签管理 ──────────────────────────────────────────────────

void SidebarWidget::updateTagList()
{
    m_tagList->clear();
    auto *app = ShorthandApplication::instance();
    QList<TagData> tags = app->tagManager()->getAllTags();
    for (const TagData &tag : tags) {
        QListWidgetItem *item = new QListWidgetItem("  " + tag.name);
        item->setData(Qt::UserRole, tag.name);
        item->setData(Qt::UserRole + 1, tag.id);
        m_tagList->addItem(item);
    }
}

// ─── 右键菜单 ────────────────────────────────────────────────

void SidebarWidget::showTagContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_tagList->itemAt(pos);
    QMenu menu(this);

    QAction *actNew = menu.addAction(tr("新建标签"));
    connect(actNew, &QAction::triggered, this, &SidebarWidget::onCreateTag);

    if (item) {
        m_tagList->setCurrentItem(item);
        menu.addSeparator();
        QAction *actRename = menu.addAction(tr("重命名"));
        connect(actRename, &QAction::triggered, this, &SidebarWidget::onRenameTag);
        QAction *actDelete = menu.addAction(tr("删除"));
        connect(actDelete, &QAction::triggered, this, &SidebarWidget::onDeleteTag);
    }

    menu.exec(m_tagList->mapToGlobal(pos));
}

void SidebarWidget::onCreateTag()
{
    auto *app = ShorthandApplication::instance();
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("新建标签"),
                                          tr("请输入标签名称："),
                                          QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    name = name.trimmed();
    if (app->tagManager()->getTagByName(name).id > 0) {
        QMessageBox::warning(this, tr("重复标签"),
                             tr("标签「%1」已存在，请使用其他名称。").arg(name));
        return;
    }
    app->tagManager()->createTag(name);
}

void SidebarWidget::onRenameTag()
{
    QListWidgetItem *item = m_tagList->currentItem();
    if (!item) return;

    QString oldName = item->data(Qt::UserRole).toString();
    int tagId = item->data(Qt::UserRole + 1).toInt();
    if (tagId <= 0) return;

    auto *app = ShorthandApplication::instance();
    bool ok = false;
    QString newName = QInputDialog::getText(this, tr("重命名标签"),
                                             tr("请输入新名称："),
                                             QLineEdit::Normal, oldName, &ok);
    if (!ok || newName.trimmed().isEmpty() || newName.trimmed() == oldName) return;

    newName = newName.trimmed();
    TagData existing = app->tagManager()->getTagByName(newName);
    if (existing.id > 0 && existing.id != tagId) {
        QMessageBox::warning(this, tr("重复标签"),
                             tr("标签「%1」已存在，请使用其他名称。").arg(newName));
        return;
    }
    TagData current = app->tagManager()->getTag(tagId);
    app->tagManager()->updateTag(tagId, newName, current.color);
}

void SidebarWidget::onDeleteTag()
{
    QListWidgetItem *item = m_tagList->currentItem();
    if (!item) return;

    QString tagName = item->data(Qt::UserRole).toString();
    int tagId = item->data(Qt::UserRole + 1).toInt();
    if (tagId <= 0) return;

    auto *app = ShorthandApplication::instance();
    QMessageBox::StandardButton btn = QMessageBox::question(
        this, tr("删除标签"),
        tr("确定要删除标签「%1」吗？\n删除后，关联的笔记和待办将不再包含此标签。").arg(tagName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (btn == QMessageBox::Yes) {
        app->tagManager()->deleteTag(tagId);
    }
}
