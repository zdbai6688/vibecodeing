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
#include <QDebug>

SidebarWidget::SidebarWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(200);
    initUI();
    updateStyleSheet();

    // 监听主题变化
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged,
            this, &SidebarWidget::updateStyleSheet);

    auto *app = ShorthandApplication::instance();
    connect(app->tagManager(), &TagManager::dataChanged, this, &SidebarWidget::updateTagList);
    connect(app->noteManager(), &NoteManager::dataChanged, this, [this, app]() {
        updateBadge(app->noteManager()->noteCount(), app->todoManager()->pendingCount());
    });
}

void SidebarWidget::updateStyleSheet()
{
    auto *helper = DGuiApplicationHelper::instance();
    bool dark = (helper->themeType() == DGuiApplicationHelper::DarkType);

    QString gradStart = dark ? "#1f2a3a" : "#D8DCF0";
    QString gradEnd   = dark ? "#2d3a4a" : "#C8CCE2";
    QString textColor = dark ? "#E0E0E0" : "#222222";
    QString textMuted = dark ? "#AAAAAA" : "#666666";
    QString accent    = dark ? "#78A9FF" : "#2178E5";
    QString accentBg  = dark ? "#78A9FF" : "#2178E5";
    QString hoverBg   = dark ? "rgba(120,169,255,0.12)" : "rgba(33,120,229,0.08)";
    QString btnBg     = dark ? "#353535" : "#F0F0F0";
    QString btnBgHov  = dark ? "#454545" : "#E0E0E0";
    QString badgeBg   = accent;
    QString badgeText = dark ? "#1e1e1e" : "#FFFFFF";

    setStyleSheet(QString(R"(
        SidebarWidget {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 %1, stop:1 %2);
        }
        QPushButton#navBtn {
            background: transparent; border: none; border-radius: 6px;
            padding: 8px 12px; text-align: left; font-size: 13px; color: %3;
        }
        QPushButton#navBtn:hover { background: %5; }
        QPushButton#navBtn:checked {
            background: %6; color: %8; font-weight: 600;
        }
        QPushButton#navBtn:checked QLabel { color: %8; }
        QPushButton#newBtn {
            background: %7; color: %3; border: none; border-radius: 6px;
            padding: 8px; font-size: 13px;
        }
        QPushButton#newBtn:hover { background: %9; }
        QLabel#sectionLabel {
            color: %4; font-size: 11px; font-weight: 600;
            padding: 4px 12px 2px 12px; letter-spacing: 0.5px;
        }
        QLabel#badge {
            color: %4; font-size: 11px; padding: 0 4px;
        }
    )").arg(gradStart, gradEnd, textColor, textMuted, hoverBg,
            accentBg, btnBg, badgeText, btnBgHov));

    // 更新 badge 和 tag 列表样式
    if (m_badgeNotes) {
        m_badgeNotes->setStyleSheet(QString(R"(
            QLabel {
                background: %1; color: %2; font-size: 11px; font-weight: 600;
                border-radius: 11px; padding: 0;
            }
        )").arg(badgeBg, badgeText));
    }
    if (m_badgeTodos) {
        m_badgeTodos->setStyleSheet(QString(R"(
            QLabel {
                background: %1; color: %2; font-size: 11px; font-weight: 600;
                border-radius: 11px; padding: 0;
            }
        )").arg(badgeBg, badgeText));
    }

    if (m_tagList) {
        m_tagList->setStyleSheet(QString(R"(
            QListWidget { background: transparent; border: none; padding: 0 6px; }
            QListWidget::item {
                border-radius: 6px; padding: 6px 12px; font-size: 13px; color: %1;
            }
            QListWidget::item:hover { background: %2; }
            QListWidget::item:selected { background: %3; color: %4; }
        )").arg(textColor, hoverBg, accentBg,
               dark ? "#1e1e1e" : "#FFFFFF"));
    }
}

QPushButton *SidebarWidget::makeNavBtn(const QString &icon, const QString &text, QLabel *&badgeLabel)
{
    QWidget *row = new QWidget(this);
    QHBoxLayout *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(12, 0, 12, 0);
    rowLayout->setSpacing(6);

    QPushButton *btn = new QPushButton(icon + "  " + text, this);
    btn->setObjectName("navBtn");
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(34);

    badgeLabel = new QLabel("", this);
    badgeLabel->setFixedSize(22, 22);
    badgeLabel->setAlignment(Qt::AlignCenter);
    badgeLabel->hide();

    rowLayout->addWidget(btn, 1);
    rowLayout->addWidget(badgeLabel);
    m_layout->addWidget(row);
    return btn;
}

void SidebarWidget::addSectionLabel(const QString &text)
{
    DLabel *label = new DLabel(text, this);
    label->setObjectName("sectionLabel");
    label->setFixedHeight(20);
    m_layout->addWidget(label);
    m_layout->addSpacing(2);
}

void SidebarWidget::initUI()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 12, 0, 12);
    m_layout->setSpacing(0);

    // Logo
    DLabel *logo = new DLabel(tr("UOS 速记"), this);
    logo->setStyleSheet("font-size: 16px; font-weight: 700; padding: 8px 16px 16px 16px;");
    // 使用 DPalette 设置主题色
    QPalette pa = logo->palette();
    auto *dguiHelper = DGuiApplicationHelper::instance();
    if (dguiHelper) {
        QColor accentColor = dguiHelper->themeType() == DGuiApplicationHelper::DarkType
            ? QColor("#78A9FF") : QColor("#2178E5");
        pa.setColor(QPalette::WindowText, accentColor);
    } else {
        pa.setColor(QPalette::WindowText, QColor("#2178E5"));
    }
    logo->setPalette(pa);
    m_layout->addWidget(logo);

    // 核心功能
    addSectionLabel(tr("【核心功能】"));
    QLabel *dummy = nullptr;
    m_btnNotes = makeNavBtn("📝", tr("笔记"), m_badgeNotes);
    m_btnNotes->setChecked(true);
    m_btnTodos = makeNavBtn("✅", tr("待办事项"), m_badgeTodos);
    m_btnMeetings = makeNavBtn("🎤", tr("会议记录"), dummy);
    m_layout->addSpacing(12);

    // 标签筛选
    addSectionLabel(tr("【标签筛选】"));
    m_tagList = new QListWidget(this);
    m_tagList->setObjectName("tagList");
    m_tagList->setFrameShape(QFrame::NoFrame);
    m_tagList->setMaximumHeight(130);
    m_layout->addWidget(m_tagList);
    m_layout->addSpacing(12);

    // 归档
    addSectionLabel(tr("【归档】"));
    m_btnWeekly = makeNavBtn("📊", tr("周报"), dummy);
    m_btnCompleted = makeNavBtn("✅", tr("已完成待办"), dummy);
    m_btnTrash = makeNavBtn("🗑", tr("最近删除"), dummy);

    m_layout->addStretch();

    // 设置
    m_btnSettings = makeNavBtn("⚙", tr("设置"), dummy);
    m_layout->addSpacing(8);

    // 新建按钮
    m_btnNewNote = new QPushButton(tr("+ 新建"), this);
    m_btnNewNote->setObjectName("newBtn");
    m_btnNewNote->setCursor(Qt::PointingHandCursor);
    m_btnNewNote->setFixedHeight(38);
    QHBoxLayout *newRow = new QHBoxLayout();
    newRow->setContentsMargins(12, 0, 12, 0);
    newRow->addWidget(m_btnNewNote);
    m_layout->addLayout(newRow);
    m_layout->addSpacing(8);

    // 信号
    connect(m_btnNotes, &QPushButton::clicked, this, [this]() { setActiveSection(0); emit notesClicked(); });
    connect(m_btnTodos, &QPushButton::clicked, this, [this]() { setActiveSection(1); emit todosClicked(); });
    connect(m_btnMeetings, &QPushButton::clicked, this, [this]() { setActiveSection(2); emit meetingsClicked(); });
    connect(m_btnWeekly, &QPushButton::clicked, this, [this]() { setActiveSection(5); emit weeklyClicked(); });
    connect(m_btnCompleted, &QPushButton::clicked, this, [this]() { setActiveSection(4); emit completedTodosClicked(); });
    connect(m_btnTrash, &QPushButton::clicked, this, [this]() { setActiveSection(3); emit trashClicked(); });
    connect(m_btnSettings, &QPushButton::clicked, this, [this]() { setActiveSection(6); emit settingsClicked(); });
    connect(m_btnNewNote, &QPushButton::clicked, this, [this]() { emit newNoteClicked(); });
    connect(m_tagList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        QString tag = item->data(Qt::UserRole).toString();
        emit tagClicked(tag);
    });

    updateTagList();
    updateBadge(0, 0);
}

void SidebarWidget::setActiveSection(int index)
{
    m_btnNotes->setChecked(index == 0);
    m_btnTodos->setChecked(index == 1);
    m_btnMeetings->setChecked(index == 2);
    m_btnTrash->setChecked(index == 3);
    m_btnCompleted->setChecked(index == 4);
    m_btnWeekly->setChecked(index == 5);
    m_btnSettings->setChecked(index == 6);
}

void SidebarWidget::updateBadge(int notes, int todos)
{
    if (notes > 0) {
        m_badgeNotes->setText(QString::number(notes));
        m_badgeNotes->show();
    } else {
        m_badgeNotes->hide();
    }
    if (todos > 0) {
        m_badgeTodos->setText(QString::number(todos));
        m_badgeTodos->show();
    } else {
        m_badgeTodos->hide();
    }
}

void SidebarWidget::updateTagList()
{
    m_tagList->clear();
    auto *app = ShorthandApplication::instance();
    QStringList tags = app->tagManager()->allTagNames();
    for (const QString &tag : tags) {
        QListWidgetItem *item = new QListWidgetItem("  " + tag);
        item->setData(Qt::UserRole, tag);
        m_tagList->addItem(item);
    }
}
