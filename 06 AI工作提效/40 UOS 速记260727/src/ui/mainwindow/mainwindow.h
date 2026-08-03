// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <DMainWindow>
#include <QStackedWidget>
#include <QWidget>
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
    void showMiddleWidget(QWidget *w);
    void updateCreateButtonTooltip();

    SidebarWidget *m_sidebar;
    QWidget *m_middlePanel;
    QStackedWidget *m_middleStack;
    NoteListWidget *m_noteList;
    NoteEditorWidget *m_editor;
    TodoWidget *m_todoWidget;
    MeetingWidget *m_meetingWidget;
    WeeklyReportWidget *m_weeklyWidget;
    QuickEntryDialog *m_quickEntry;
    SettingsDialog *m_settingsDialog;
    GlobalShortcutManager *m_globalShortcut;
    QWidget *m_blankEditor;
    QHBoxLayout *m_mainLayout;
    QFrame *m_sep1;

    // 标题栏按钮
    DToolButton *m_createBtn;
    DToolButton *m_moreBtn;
    DToolButton *m_exportBtn;
    DMenu *m_createMenu;
};

#endif // MAINWINDOW_H
