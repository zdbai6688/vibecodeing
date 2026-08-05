// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "sidebarwidget.h"
#include "notelistwidget.h"
#include "ui/editor/noteeditorwidget.h"
#include "ui/todo/todowidget.h"
#include "ui/meeting/meetingwidget.h"
#include "ui/weekly/weeklyreportwidget.h"
#include "ui/quickentry/quickentrydialog.h"
#include "ui/settings/settingsdialog.h"
#include "application/shorthandapplication.h"
#include "core/notemanager.h"
#include "core/tagmanager.h"
#include "core/todomanager.h"
#include "ui/tray/traymanager.h"
#include "services/globalshortcutmanager.h"
#include "globaldef.h"
#include "ui/desktop/desktopmodemanager.h"
#include <QPair>

#include <DTitlebar>
#include <DToolButton>
#include <DWidgetUtil>
#include <DGuiApplicationHelper>
#include <QShortcut>
#include <QIcon>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QDebug>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSettings>
#include <QAction>
#include <QDialog>

// 快捷键 ID 枚举
enum ShortcutId {
    ShortcutQuickEntry = 1001
};

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
{
    setWindowTitle(tr("UOS速记"));
    setObjectName("MainWindow");
    setMinimumSize(1100, 700);   // PRD §5.2: 最小窗口 1100×700

    qInfo() << "[MainWindow] initUI";
    initUI();
    initConnections();
    setupGlobalShortcut();
    qInfo() << "[MainWindow] 构造完成";
}

MainWindow::~MainWindow()
{
    if (m_globalShortcut) {
        m_globalShortcut->unregisterAll();
    }
}

void MainWindow::initUI()
{
    DTitlebar *titlebar = this->titlebar();
    titlebar->setTitle("");
    titlebar->setIcon(QIcon::fromTheme("uos-shorthand"));

    // ===== 统一新建入口（PRD §4.1）=====
    m_createBtn = new DToolButton(this);
    m_createBtn->setIcon(QIcon::fromTheme("list-add"));
    m_createBtn->setToolTip(tr("新建 (Ctrl+N)"));
    m_createBtn->setFixedSize(32, 32);

    // 新建菜单（按上下文创建）
    m_createMenu = new DMenu(this);
    QAction *actNewNote = m_createMenu->addAction(QIcon::fromTheme("document-new"),
                                                   tr("新建笔记 (Ctrl+N)"));
    actNewNote->setData("note");
    QAction *actNewTodo = m_createMenu->addAction(QIcon::fromTheme("task-new"),
                                                   tr("新建待办 (Ctrl+Shift+N)"));
    actNewTodo->setData("todo");
    QAction *actNewMeeting = m_createMenu->addAction(QIcon::fromTheme("audio-input-microphone"),
                                                      tr("新建会议"));
    actNewMeeting->setData("meeting");
    QAction *actNewNoteWithTag = m_createMenu->addAction(QIcon::fromTheme("document-new"),
                                                          tr("新建笔记并选择标签"));
    actNewNoteWithTag->setData("note-tag");

    connect(m_createMenu, &DMenu::triggered, this, [this](QAction *act) {
        QString data = act->data().toString();
        if (data == "note") onNewNote();
        else if (data == "todo") onNewTodo();
        else if (data == "meeting") onNewMeeting();
        else if (data == "note-tag") onNewNoteWithTag();
    });

    // 点击新建按钮：按当前上下文创建（PRD §4.1）
    connect(m_createBtn, &DToolButton::clicked, this, &MainWindow::onUnifiedCreate);

    titlebar->addWidget(m_createBtn, Qt::AlignRight);

    // 导出按钮
    m_exportBtn = new DToolButton(this);
    m_exportBtn->setIcon(QIcon::fromTheme("document-save"));
    m_exportBtn->setToolTip(tr("导出"));
    m_exportBtn->setFixedSize(32, 32);
    titlebar->addWidget(m_exportBtn, Qt::AlignRight);

    // 更多按钮
    m_moreBtn = new DToolButton(this);
    m_moreBtn->setText("⋯");
    m_moreBtn->setToolTip(tr("更多"));
    m_moreBtn->setFixedSize(32, 32);
    DMenu *moreMenu = new DMenu(this);
    QAction *actQuickEntry = moreMenu->addAction(tr("快速录入 (Alt+Space)"));
    connect(actQuickEntry, &QAction::triggered, this, &MainWindow::onShowQuickEntry);
    moreMenu->addSeparator();
    QAction *actSettings = moreMenu->addAction(QIcon::fromTheme("settings-configure"),
                                                tr("设置"));
    connect(actSettings, &QAction::triggered, this, &MainWindow::onShowSettings);
    moreMenu->addSeparator();
    QAction *actDesktopMode = moreMenu->addAction(tr("桌面便签模式"));
    connect(actDesktopMode, &QAction::triggered, this, &MainWindow::onToggleDesktopMode);
    m_moreBtn->setMenu(moreMenu);
    m_moreBtn->setPopupMode(DToolButton::InstantPopup);
    titlebar->addWidget(m_moreBtn, Qt::AlignRight);

    // ===== 主内容区 - 三栏布局 =====
    QWidget *centralWidget = new QWidget(this);
    m_mainLayout = new QHBoxLayout(centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 左侧导航栏
    m_sidebar = new SidebarWidget(this);
    m_mainLayout->addWidget(m_sidebar);

    m_sep1 = new QFrame(this);
    m_sep1->setFrameShape(QFrame::VLine);
    m_sep1->setObjectName("separator1");
    m_sep1->setStyleSheet("#separator1 { color: palette(midlight); background: palette(midlight); width: 1px; }");
    m_mainLayout->addWidget(m_sep1);

    // 中间内容栏
    m_middlePanel = new QWidget(this);
    m_middlePanel->setObjectName("middlePanel");
    m_middlePanel->setStyleSheet("#middlePanel { background: palette(base); }");
    QVBoxLayout *middleLayout = new QVBoxLayout(m_middlePanel);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(0);

    m_middleStack = new QStackedWidget(this);
    m_noteList = new NoteListWidget(this);
    m_todoWidget = new TodoWidget(this);
    m_meetingWidget = new MeetingWidget(this);
    m_weeklyWidget = new WeeklyReportWidget(this);
    m_middleStack->addWidget(m_noteList);
    m_middleStack->addWidget(m_todoWidget);
    m_middleStack->addWidget(m_meetingWidget);
    m_middleStack->addWidget(m_weeklyWidget);
    middleLayout->addWidget(m_middleStack);
    m_mainLayout->addWidget(m_middlePanel, 1);

    QFrame *sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setObjectName("separator2");
    sep2->setStyleSheet("#separator2 { color: palette(midlight); background: palette(midlight); width: 1px; }");
    m_mainLayout->addWidget(sep2);

    // 右侧编辑器
    qInfo() << "[MainWindow] 创建编辑器";
    m_editor = new NoteEditorWidget(this);
    qInfo() << "[MainWindow] 编辑器创建完成";
    m_editor->setObjectName("editorPanel");
    m_editor->setStyleSheet("#editorPanel { background: palette(base); }");

    m_blankEditor = new QWidget(this);
    m_blankEditor->setObjectName("blankEditor");
    m_blankEditor->setStyleSheet("#blankEditor { background: palette(base); }");
    QVBoxLayout *blankLayout = new QVBoxLayout(m_blankEditor);
    blankLayout->setAlignment(Qt::AlignCenter);
    blankLayout->setSpacing(12);
    QLabel *blankIcon = new QLabel(tr("📄"), m_blankEditor);
    blankIcon->setStyleSheet("font-size: 48px;");
    blankIcon->setAlignment(Qt::AlignCenter);
    blankLayout->addWidget(blankIcon);
    QLabel *blankHint = new QLabel(tr("选择左侧笔记开始编辑，或点击「+」新建"), m_blankEditor);
    blankHint->setStyleSheet("font-size: 14px; color: palette(windowText);");
    blankHint->setAlignment(Qt::AlignCenter);
    blankLayout->addWidget(blankHint);
    QLabel *blankSubHint = new QLabel(tr("Ctrl+N 新建笔记  ·  Ctrl+Shift+N 新建待办  ·  Alt+Space 快速录入"),
                                       m_blankEditor);
    blankSubHint->setStyleSheet("font-size: 12px; color: palette(buttonText);");
    blankSubHint->setAlignment(Qt::AlignCenter);
    blankLayout->addWidget(blankSubHint);

    m_mainLayout->addWidget(m_editor, 2);
    m_mainLayout->addWidget(m_blankEditor, 2);
    m_blankEditor->show();
    m_editor->hide();

    // 初始化设置对话框（快速录入窗口按需创建，支持多窗口并存）
    m_settingsDialog = new SettingsDialog(this);

    setCentralWidget(centralWidget);
}

void MainWindow::initConnections()
{
    auto *app = ShorthandApplication::instance();

    // 侧边栏导航
    connect(m_sidebar, &SidebarWidget::notesClicked, this, &MainWindow::onSwitchToNotes);
    connect(m_sidebar, &SidebarWidget::todosClicked, this, &MainWindow::onSwitchToTodos);
    connect(m_sidebar, &SidebarWidget::meetingsClicked, this, &MainWindow::onSwitchToMeetings);
    connect(m_sidebar, &SidebarWidget::weeklyClicked, this, &MainWindow::onSwitchToWeekly);
    connect(m_sidebar, &SidebarWidget::trashClicked, this, &MainWindow::onSwitchToTrash);
    connect(m_sidebar, &SidebarWidget::completedTodosClicked, this, &MainWindow::onSwitchToCompletedTodos);
    connect(m_sidebar, &SidebarWidget::settingsClicked, this, &MainWindow::onShowSettings);
    connect(m_sidebar, &SidebarWidget::tagClicked, this, &MainWindow::onSwitchToTag);
    connect(m_sidebar, &SidebarWidget::newNoteClicked, this, &MainWindow::onNewNote);
    connect(m_sidebar, &SidebarWidget::collapseChanged, this, &MainWindow::onSidebarCollapseChanged);

    // 笔记列表
    connect(m_noteList, &NoteListWidget::noteSelected, this, &MainWindow::onNoteSelected);
    // 待办
    connect(m_todoWidget, &TodoWidget::todoSelected, this, &MainWindow::onTodoSelected);
    // 界面切换时更新创建按钮提示
    connect(m_middleStack, &QStackedWidget::currentChanged, this, [this]() {
        updateCreateButtonTooltip();
    });

    // 数据变更自动刷新
    connect(app->noteManager(), &NoteManager::dataChanged, this, [this]() {
        m_noteList->refresh();
    });

    // 托盘信号连接（PRD §3 系统集成）
    connect(app->trayManager(), &TrayManager::showMainWindowRequested, this, [this]() {
        show(); raise(); activateWindow();
    });
    connect(app->trayManager(), &TrayManager::quickEntryRequested, this, &MainWindow::onShowQuickEntry);
    connect(app->trayManager(), &TrayManager::quitRequested, qApp, &QApplication::quit);
    connect(app->trayManager(), &TrayManager::toggleDesktopModeRequested, this, &MainWindow::onToggleDesktopMode);

    // 桌面模式信号连接
    connect(app->desktopModeManager(), &DesktopModeManager::desktopModeEntered, this, [this, app]() {
        app->trayManager()->updateDesktopModeAction(true);
        QList<QPair<int, QString>> notes;
        for (int id : app->desktopModeManager()->stickyNoteIds()) {
            NoteData d = app->noteManager()->getNote(id);
            if (d.id > 0) notes.append({id, d.title.isEmpty() ? d.content.left(30) : d.title.left(30)});
        }
        app->trayManager()->updateStickyNotesSubmenu(notes);
    });
    connect(app->desktopModeManager(), &DesktopModeManager::desktopModeExited, this, [this, app]() {
        app->trayManager()->updateDesktopModeAction(false);
    });

    // 托盘便签点击
    connect(app->trayManager(), &TrayManager::showStickyNoteRequested, this, [this](int noteId) {
        focusNote(noteId);
        show(); raise(); activateWindow();
    });
}

void MainWindow::setupGlobalShortcut()
{
    m_globalShortcut = ShorthandApplication::instance()->globalShortcut();

    // 注册全局快捷键（设置页可自定义，读取已保存的快捷键）
    applyGlobalShortcut();

    // 设置页保存后立即重新注册，保证改键即时生效
    connect(m_settingsDialog, &SettingsDialog::accepted,
            this, &MainWindow::applyGlobalShortcut);

    // Ctrl+N 新建笔记（PRD §4.1）
    QShortcut *newNoteShortcut = new QShortcut(QKeySequence(SHORTCUT_NEW_NOTE), this);
    connect(newNoteShortcut, &QShortcut::activated, this, &MainWindow::onNewNote);

    // Ctrl+Shift+N 新建待办（PRD §4.1）
    QShortcut *newTodoShortcut = new QShortcut(QKeySequence("Ctrl+Shift+N"), this);
    connect(newTodoShortcut, &QShortcut::activated, this, &MainWindow::onNewTodo);

    // Ctrl+F 搜索
    QShortcut *searchShortcut = new QShortcut(QKeySequence(SHORTCUT_SEARCH), this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() {
        m_noteList->setFocus();
    });

    // Ctrl+S 保存
    QShortcut *saveShortcut = new QShortcut(QKeySequence(SHORTCUT_SAVE), this);
    connect(saveShortcut, &QShortcut::activated, this, [this]() {
        m_editor->onSave();
    });
}

void MainWindow::applyGlobalShortcut()
{
    // 从设置读取用户自定义快捷键（P4-T3），默认 Alt+Space（PRD §4.3）
    QSettings settings;
    QString shortcutKey = settings.value("shortcut/quick_entry",
                                         QString(SHORTCUT_QUICK_ENTRY)).toString();
    QKeySequence seq(shortcutKey);

    if (!m_globalShortcut) {
        m_globalShortcut = ShorthandApplication::instance()->globalShortcut();
    }

    // 清理旧的信号连接，避免重复注册造成多次触发
    disconnect(m_globalShortcut, nullptr, this, nullptr);

    // 先注销旧快捷键（设置变更后重新注册）
    m_globalShortcut->unregisterShortcut(ShortcutQuickEntry);

    // 删除旧的回退快捷键
    if (m_fallbackShortcut) {
        delete m_fallbackShortcut;
        m_fallbackShortcut = nullptr;
    }

    bool registered = m_globalShortcut->registerShortcut(seq, ShortcutQuickEntry);
    if (registered) {
        connect(m_globalShortcut, &GlobalShortcutManager::shortcutActivated,
                this, [this](quint32 id) {
            if (id == ShortcutQuickEntry) {
                onShowQuickEntry();
            }
        });
        qInfo() << "[MainWindow] 全局快捷键注册成功:" << shortcutKey;
    } else {
        // Wayland / 注册失败时退化为应用内快捷键
        qWarning() << "[MainWindow] 全局快捷键注册失败，使用应用内快捷键回退:" << shortcutKey;
        m_fallbackShortcut = new QShortcut(seq, this);
        connect(m_fallbackShortcut, &QShortcut::activated,
                this, &MainWindow::onShowQuickEntry);
        qInfo() << "[MainWindow] 使用应用内快捷键(回退):" << shortcutKey;
    }
}

void MainWindow::onSidebarCollapseChanged(bool collapsed)
{
    // 调整分隔线可见性
    m_sep1->setVisible(!collapsed);
    updateCreateButtonTooltip();
}

void MainWindow::updateCreateButtonTooltip()
{
    QWidget *current = m_middleStack->currentWidget();
    if (current == m_todoWidget) {
        m_createBtn->setToolTip(tr("新建待办 (Ctrl+Shift+N)"));
    } else if (current == m_meetingWidget) {
        m_createBtn->setToolTip(tr("新建会议"));
    } else {
        m_createBtn->setToolTip(tr("新建笔记 (Ctrl+N)"));
    }
}

void MainWindow::onUnifiedCreate()
{
    // PRD §4.1: "+" 按当前上下文新建
    QWidget *current = m_middleStack->currentWidget();
    if (current == m_todoWidget) {
        onNewTodo();
    } else if (current == m_meetingWidget) {
        onNewMeeting();
    } else {
        onNewNote();
    }
}

void MainWindow::onNewNote()
{
    NoteData note;
    note.title = tr("无标题笔记");
    note.content = "";
    note.contentType = "markdown";
    note.creationDatetime = QDateTime::currentSecsSinceEpoch();
    note.modificationDatetime = note.creationDatetime;

    auto *app = ShorthandApplication::instance();
    int id = app->noteManager()->createNote(note);
    if (id > 0) {
        m_noteList->selectNote(id);
        m_editor->loadNote(id);
        m_noteList->refresh();
        m_sidebar->setActiveSection(0);
        showMiddleWidget(m_noteList);
        m_blankEditor->hide();
        m_editor->show();
    }
}

void MainWindow::onNewTodo()
{
    auto *app = ShorthandApplication::instance();
    Q_UNUSED(app);
    // 切换到待办视图并聚焦输入
    m_sidebar->setActiveSection(1);
    showMiddleWidget(m_todoWidget);
    m_todoWidget->refresh();
    m_todoWidget->focusNewTodoInput();
}

void MainWindow::onNewMeeting()
{
    m_sidebar->setActiveSection(2);
    showMiddleWidget(m_meetingWidget);
    m_meetingWidget->refresh();
    // 触发新建会议
    m_meetingWidget->onNewMeeting();
}

void MainWindow::onNewNoteWithTag()
{
    // 新建笔记并弹出标签选择
    onNewNote();
    // 后续可在 NoteEditorWidget 中处理标签选择
}

void MainWindow::showMiddleWidget(QWidget *w)
{
    m_middleStack->setCurrentWidget(w);
}

void MainWindow::onNoteSelected(int noteId)
{
    if (noteId > 0) {
        m_editor->loadNote(noteId);
        m_blankEditor->hide();
        m_editor->show();
    }
}

void MainWindow::onTodoSelected(int todoId)
{
    if (todoId > 0) {
        m_editor->loadNote(todoId);
        m_blankEditor->hide();
        m_editor->show();
    }
}

void MainWindow::onShowQuickEntry()
{
    // 清理已销毁的窗口，避免悬挂指针
    for (auto it = m_quickEntries.begin(); it != m_quickEntries.end();) {
        if (!*it) {
            it = m_quickEntries.erase(it);
        } else {
            ++it;
        }
    }

    // 同时打开的紧凑窗口上限（与桌面便签默认上限一致）
    const int maxQuickEntries = 6;
    if (m_quickEntries.size() >= maxQuickEntries) {
        m_quickEntries.last()->setFocus();
        return;
    }

    // 每次触发都新开一个紧凑窗口，级联摆放，内容互不干扰、独立保存
    QuickEntryDialog *entry = new QuickEntryDialog(this);
    entry->setCascadeIndex(m_quickEntries.size());
    connect(entry, &QuickEntryDialog::dismissed, this, [this, entry]() {
        m_quickEntries.removeAll(entry);
        entry->deleteLater();
    });
    m_quickEntries.append(entry);
    entry->setFocus();
}

void MainWindow::loadInitialNotes()
{
    onSwitchToNotes();
    auto *app = ShorthandApplication::instance();
    QList<NoteData> notes = app->noteManager()->getAllNotes();
    if (!notes.isEmpty()) {
        int firstId = notes.first().id;
        m_noteList->selectNote(firstId);
    }
}

void MainWindow::onToggleDesktopMode()
{
    auto *app = ShorthandApplication::instance();
    if (app->desktopModeManager()->isDesktopMode()) {
        app->desktopModeManager()->exitDesktopMode();
        show();
        raise();
        activateWindow();
    } else {
        hide();
        app->desktopModeManager()->enterDesktopMode();
    }
}

void MainWindow::focusNote(int noteId)
{
    onSwitchToNotes();
    NoteData note = ShorthandApplication::instance()->noteManager()->getNote(noteId);
    if (note.id > 0) {
        m_noteList->selectNote(noteId);
        m_editor->loadNote(noteId);
    }
}

void MainWindow::onShowSettings()
{
    if (!m_settingsDialog) return;

    m_settingsDialog->loadSettings();
    int code = m_settingsDialog->exec();
    if (code == QDialog::Accepted) {
        // 设置仅在用户点击确定后保存（取消不落盘）
        m_settingsDialog->saveSettings();
    }
}

void MainWindow::onSwitchToNotes()
{
    m_sidebar->setActiveSection(0);
    showMiddleWidget(m_noteList);
    m_noteList->setMode(NoteListWidget::AllNotes);
    m_noteList->refresh();
}

void MainWindow::onSwitchToTodos()
{
    m_sidebar->setActiveSection(1);
    showMiddleWidget(m_todoWidget);
    // 回到待办视图时清除标签筛选
    m_todoWidget->clearFilterTags();
    m_todoWidget->refresh();
    m_todoWidget->focusNewTodoInput();
}

void MainWindow::onSwitchToMeetings()
{
    m_sidebar->setActiveSection(2);
    showMiddleWidget(m_meetingWidget);
    m_meetingWidget->refresh();
}

void MainWindow::onSwitchToWeekly()
{
    m_sidebar->setActiveSection(5);
    showMiddleWidget(m_weeklyWidget);
    m_weeklyWidget->refresh();
}

void MainWindow::onSwitchToTrash()
{
    m_sidebar->setActiveSection(3);
    showMiddleWidget(m_noteList);
    m_noteList->setMode(NoteListWidget::Trash);
    m_noteList->refresh();
}

void MainWindow::onSwitchToCompletedTodos()
{
    m_sidebar->setActiveSection(4);
    showMiddleWidget(m_todoWidget);
    m_todoWidget->refresh();
}

void MainWindow::onSwitchToTag(const QString &tag)
{
    // 若当前停留在待办视图，标签筛选作用于待办列表
    if (m_sidebar->isTodoActive()) {
        onSwitchToTodos();
        m_todoWidget->setFilterTags({tag});
        m_todoWidget->refresh();
    } else {
        onSwitchToNotes();
        m_noteList->setMode(NoteListWidget::TagFilter);
        m_noteList->setFilterTag(tag);
        m_noteList->refresh();
    }
    // 标签视图激活：取消导航按钮高亮，仅高亮当前标签（与导航互斥）
    m_sidebar->activateTag(tag);
}

void MainWindow::onSearch(const QString &keyword)
{
    m_noteList->setSearchKeyword(keyword);
    m_noteList->refresh();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (ShorthandApplication::instance()->trayManager()->isAvailable()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        bool dismissedAny = false;
        for (QuickEntryDialog *entry : m_quickEntries) {
            if (entry && entry->isVisible()) {
                // 关闭紧凑窗口，未保存内容保留为草稿（下一个新窗口可恢复）
                entry->dismissWithDraft();
                dismissedAny = true;
            }
        }
        if (dismissedAny) {
            event->accept();
            return;
        }
    }
    DMainWindow::keyPressEvent(event);
}
