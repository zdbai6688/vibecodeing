// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SIDEBARWIDGET_H
#define SIDEBARWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QPropertyAnimation>

#include <DLabel>

DWIDGET_USE_NAMESPACE

/**
 * @brief 侧边导航栏 - 桌面模式三栏布局左栏
 *
 * 符合 PRD §3.1–3.2 规范：
 * - 三组导航：核心功能 / 标签筛选 / 归档
 * - 可折叠 200↔64px（图标模式）
 * - 角标规则 ≤99 显示数字，>99 显示 "99+"
 * - 使用 DTK6 DPalette 语义色 token
 * - 扁平背景
 */
class SidebarWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(SidebarWidget)
    Q_PROPERTY(int sidebarWidth READ sidebarWidth WRITE setSidebarWidth)

public:
    explicit SidebarWidget(QWidget *parent = nullptr);

    void setActiveSection(int index);
    void activateTag(const QString &tag);
    bool isCollapsed() const { return m_collapsed; }
    bool isTodoActive() const { return m_activeSection == 1; }
    int sidebarWidth() const { return m_currentWidth; }
    void setSidebarWidth(int w);

    void updateBadge(int notes, int todos);

    void toggleCollapse();
    void setCollapsed(bool collapsed);

signals:
    void notesClicked();
    void todosClicked();
    void meetingsClicked();
    void weeklyClicked();
    void trashClicked();
    void completedTodosClicked();
    void settingsClicked();
    void tagClicked(const QString &tag);
    void newNoteClicked();
    void collapseChanged(bool collapsed);

private slots:
    void showTagContextMenu(const QPoint &pos);
    void onCreateTag();
    void onRenameTag();
    void onDeleteTag();

private:
    void initUI();
    void updateTagList();
    void refreshStyleSheet();
    void updateItemVisibility();
    QString formatBadgeText(int count) const;

    // Header
    QWidget *m_headerWidget;
    QLabel *m_logoIcon;
    QLabel *m_logoText;
    QPushButton *m_collapseBtn;

    // Navigation groups
    QWidget *m_coreGroup;
    QWidget *m_tagGroup;
    QWidget *m_archiveGroup;

    // Buttons
    QPushButton *m_btnNotes;
    QPushButton *m_btnTodos;
    QPushButton *m_btnMeetings;
    QPushButton *m_btnWeekly;
    QPushButton *m_btnCompleted;
    QPushButton *m_btnTrash;
    QPushButton *m_btnSettings;
    QPushButton *m_btnNewNote;

    // Badges
    QLabel *m_badgeNotes;
    QLabel *m_badgeTodos;

    // Tags
    QListWidget *m_tagList;

    // Layout
    QVBoxLayout *m_mainLayout;

    // State
    bool m_collapsed = false;
    int m_currentWidth = 200;
    int m_activeSection = 0;
    static const int EXPANDED_WIDTH = 200;
    static const int COLLAPSED_WIDTH = 64;
};

#endif // SIDEBARWIDGET_H
