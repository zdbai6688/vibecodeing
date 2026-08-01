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
#include "globaldef.h"

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

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
{
    setWindowTitle(tr("UOS速记"));
    setObjectName("MainWindow");
    setMinimumSize(1100, 680);

    initUI();
    initConnections();
    setupGlobalShortcut();
}

MainWindow::~MainWindow() {}

void MainWindow::initUI()
{
    DTitlebar *titlebar = this->titlebar();
    titlebar->setTitle("");
    titlebar->setIcon(QIcon::fromTheme("uos-shorthand"));

    // 右上角新建按钮（悬浮）
    DToolButton *newBtn = new DToolButton(this);
    newBtn->setIcon(QIcon::fromTheme("list-add"));
    newBtn->setToolTip(tr("新建笔记 (Ctrl+N)"));
    newBtn->setFixedSize(32, 32);
    titlebar->addWidget(newBtn, Qt::AlignRight);

    // 更多下拉按钮
    DToolButton *moreBtn = new DToolButton(this);
    moreBtn->setText("⋯");
    moreBtn->setToolTip(tr("更多"));
    moreBtn->setFixedSize(32, 32);
    titlebar->addWidget(moreBtn, Qt::AlignRight);

    // 导出按钮
    DToolButton *exportBtn = new DToolButton(this);
    exportBtn->setIcon(QIcon::fromTheme("document-save"));
    exportBtn->setToolTip(tr("导出"));
    exportBtn->setFixedSize(32, 32);
    titlebar->addWidget(exportBtn, Qt::AlignRight);

    // 菜单按钮
    DToolButton *menuBtn = new DToolButton(this);
    menuBtn->setText("≡");
    menuBtn->setToolTip(tr("菜单"));
    menuBtn->setFixedSize(32, 32);
    titlebar->addWidget(menuBtn, Qt::AlignRight);

    // 主内容区 - 三栏布局
    QWidget *centralWidget = new QWidget(this);
    m_mainLayout = new QHBoxLayout(centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 左侧导航栏
    m_sidebar = new SidebarWidget(this);
    m_mainLayout->addWidget(m_sidebar);

    QFrame *sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("QFrame { color: palette(midlight); }");
    m_mainLayout->addWidget(sep1);

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
    sep2->setStyleSheet("QFrame { color: palette(midlight); }");
    m_mainLayout->addWidget(sep2);

    // 右侧编辑器
    m_editor = new NoteEditorWidget(this);
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
    QLabel *blankHint = new QLabel(tr("暂无内容"), m_blankEditor);
    blankHint->setStyleSheet("color: palette(windowText); font-size: 13px;");
    blankHint->setAlignment(Qt::AlignCenter);
    blankLayout->addWidget(blankHint);
    QLabel *blankSubHint = new QLabel(tr("在左侧选择笔记或创建新笔记"), m_blankEditor);
    blankSubHint->setStyleSheet("color: palette(placeholderText); font-size: 12px;");
    blankSubHint->setAlignment(Qt::AlignCenter);
    blankLayout->addWidget(blankSubHint);

    m_mainLayout->addWidget(m_blankEditor, 1);
    m_mainLayout->addWidget(m_editor, 1);
    m_editor->hide();

    setCentralWidget(centralWidget);

    // 设置弹窗
    m_settingsDialog = new SettingsDialog(this);

    // 快速录入
    m_quickEntry = new QuickEntryDialog(this);
    m_quickEntry->hide();

    connect(newBtn, &DToolButton::clicked, this, &MainWindow::onNewNote);
}

void MainWindow::initConnections()
{
    auto *app = ShorthandApplication::instance();

    connect(m_sidebar, &SidebarWidget::notesClicked, this, &MainWindow::onSwitchToNotes);
    connect(m_sidebar, &SidebarWidget::todosClicked, this, &MainWindow::onSwitchToTodos);
    connect(m_sidebar, &SidebarWidget::meetingsClicked, this, &MainWindow::onSwitchToMeetings);
    connect(m_sidebar, &SidebarWidget::weeklyClicked, this, &MainWindow::onSwitchToWeekly);
    connect(m_sidebar, &SidebarWidget::trashClicked, this, &MainWindow::onSwitchToTrash);
    connect(m_sidebar, &SidebarWidget::completedTodosClicked, this, &MainWindow::onSwitchToCompletedTodos);
    connect(m_sidebar, &SidebarWidget::settingsClicked, this, &MainWindow::onShowSettings);
    connect(m_sidebar, &SidebarWidget::tagClicked, this, &MainWindow::onSwitchToTag);
    connect(m_sidebar, &SidebarWidget::newNoteClicked, this, &MainWindow::onNewNote);

    connect(m_noteList, &NoteListWidget::noteSelected, this, &MainWindow::onNoteSelected);
    connect(m_todoWidget, &TodoWidget::todoStatusChanged, this, [this]() { m_todoWidget->refresh(); });
    connect(app->noteManager(), &NoteManager::dataChanged, this, [this]() { m_noteList->refresh(); });

    connect(app->trayManager(), &TrayManager::showMainWindowRequested, this, [this]() {
        show(); raise(); activateWindow();
    });
    connect(app->trayManager(), &TrayManager::quickEntryRequested, this, &MainWindow::onShowQuickEntry);
    connect(app->trayManager(), &TrayManager::quitRequested, qApp, &QApplication::quit);
}

void MainWindow::setupGlobalShortcut()
{
    QSettings settings;
    QString shortcutKey = settings.value("shortcut/quick_entry", QString(SHORTCUT_QUICK_ENTRY)).toString();
    QShortcut *quickShortcut = new QShortcut(QKeySequence(shortcutKey), this);
    connect(quickShortcut, &QShortcut::activated, this, &MainWindow::onShowQuickEntry);

    QShortcut *newNoteShortcut = new QShortcut(QKeySequence("Ctrl+N"), this);
    connect(newNoteShortcut, &QShortcut::activated, this, &MainWindow::onNewNote);
}

void MainWindow::showMiddleWidget(QWidget *w)
{
    m_middleStack->setCurrentWidget(w);
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

void MainWindow::onNoteSelected(int noteId)
{
    if (noteId > 0) {
        m_editor->loadNote(noteId);
        m_blankEditor->hide();
        m_editor->show();
    }
}

void MainWindow::onShowQuickEntry()
{
    m_quickEntry->setFocus();
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

void MainWindow::onShowSettings()
{
    m_settingsDialog->loadSettings();
    m_settingsDialog->exec();
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
    m_todoWidget->refresh();
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
    m_sidebar->setActiveSection(0);
    showMiddleWidget(m_noteList);
    m_noteList->setMode(NoteListWidget::TagFilter);
    m_noteList->setFilterTag(tag);
    m_noteList->refresh();
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
    if (event->key() == Qt::Key_Escape && m_quickEntry->isVisible()) {
        m_quickEntry->hide();
        event->accept();
        return;
    }
    DMainWindow::keyPressEvent(event);
}
