// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <DMainWindow>
#include <QStackedWidget>
#include <QWidget>
#include <QList>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFrame>
#include <DToolButton>
#include <DMenu>

class SidebarWidget;
class NoteListWidget;
class NoteEditorWidget;
class TodoWidget;
class MeetingWidget;
class WeeklyReportWidget;
class QuickEntryDialog;
class SettingsDialog;
class GlobalShortcutManager;
class QShortcut;

DWIDGET_USE_NAMESPACE

class MainWindow : public DMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY(MainWindow)

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void onShowQuickEntry();
    void onTodoSelected(int todoId);
    void loadInitialNotes();
    void focusNote(int noteId);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onNewNote();
    void onNewTodo();
    void onNewMeeting();
    void onNewNoteWithTag();
    void onUnifiedCreate();
    void onNoteSelected(int noteId);
    void onSwitchToNotes();
    void onSwitchToTodos();
    void onSwitchToMeetings();
    void onSwitchToWeekly();
    void onSwitchToTrash();
    void onSwitchToCompletedTodos();
    void onSwitchToTag(const QString &tag);
    void onSearch(const QString &keyword);
    void onShowSettings();
    void onToggleDesktopMode();
    void onSidebarCollapseChanged(bool collapsed);

private:
    void initUI();
    void initConnections();
    void setupGlobalShortcut();
    void applyGlobalShortcut();
    void showMiddleWidget(QWidget *w);
    void updateCreateButtonTooltip();
    MeetingWidget *meetingWidget();
    WeeklyReportWidget *weeklyWidget();

    SidebarWidget *m_sidebar = nullptr;
    QWidget *m_middlePanel = nullptr;
    QStackedWidget *m_middleStack = nullptr;
    NoteListWidget *m_noteList = nullptr;
    NoteEditorWidget *m_editor = nullptr;
    TodoWidget *m_todoWidget = nullptr;
    MeetingWidget *m_meetingWidget = nullptr;     // 懒加载：首次进入会议页才构造
    WeeklyReportWidget *m_weeklyWidget = nullptr; // 懒加载：首次进入周报页才构造
    QWidget *m_meetingPlaceholder = nullptr;      // 会议页占位（懒加载期显示）
    QWidget *m_weeklyPlaceholder = nullptr;       // 周报页占位（懒加载期显示）
    QList<QuickEntryDialog *> m_quickEntries;
    SettingsDialog *m_settingsDialog = nullptr;
    GlobalShortcutManager *m_globalShortcut = nullptr;
    QShortcut *m_fallbackShortcut = nullptr;
    QWidget *m_blankEditor = nullptr;
    QHBoxLayout *m_mainLayout = nullptr;
    QFrame *m_sep1 = nullptr;

    // 标题栏按钮
    DToolButton *m_createBtn = nullptr;
    DToolButton *m_moreBtn = nullptr;
    DToolButton *m_exportBtn = nullptr;
    DMenu *m_createMenu = nullptr;
};

#endif // MAINWINDOW_H
