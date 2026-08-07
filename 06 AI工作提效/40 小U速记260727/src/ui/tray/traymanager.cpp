// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "traymanager.h"
#include "application/shorthandapplication.h"
#include "core/todomanager.h"
#include "core/notemanager.h"
#include "storage/notestorage.h"

#include <QDebug>
#include <QIcon>
#include <QApplication>

TrayManager::TrayManager(QObject *parent)
    : QObject(parent)
{
    init();
}

TrayManager::~TrayManager()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
}

bool TrayManager::isAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable() && m_trayIcon != nullptr;
}

void TrayManager::showMessage(const QString &title, const QString &message)
{
    if (m_trayIcon) {
        m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
    }
}

void TrayManager::updateDesktopModeAction(bool isDesktopMode)
{
    if (m_desktopModeAction) {
        if (isDesktopMode) {
            m_desktopModeAction->setText(tr("● 🖥 退出桌面模式"));
        } else {
            m_desktopModeAction->setText(tr("🖥 切换到桌面模式"));
        }
    }
}

void TrayManager::updateStickyNotesSubmenu(const QList<QPair<int, QString>> &notes)
{
    if (!m_stickySubmenu) return;
    m_stickySubmenu->clear();

    for (const auto &pair : notes) {
        QAction *action = m_stickySubmenu->addAction(pair.second);
        int noteId = pair.first;
        connect(action, &QAction::triggered, this, [this, noteId]() {
            emit showStickyNoteRequested(noteId);
        });
    }

    m_stickySubmenu->addSeparator();
    QAction *manageAction = m_stickySubmenu->addAction(tr("▸ 管理便签…"));
    connect(manageAction, &QAction::triggered, this, [this]() {
        emit showMainWindowRequested();
    });
}

void TrayManager::init()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "系统托盘不可用";
        return;
    }

    m_trayIcon = new QSystemTrayIcon(this);
    QIcon icon;
    icon.addFile(":/icons/uos-shorthand-tray.svg");
    if (icon.isNull()) {
        icon = QIcon::fromTheme("uos-shorthand");
    }
    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip(tr("小U速记"));

    initMenu();
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &TrayManager::onTrayActivated);
}

void TrayManager::initMenu()
{
    m_trayMenu = new QMenu();

    QAction *showAction = m_trayMenu->addAction(tr("📝 打开速记"));
    connect(showAction, &QAction::triggered, this, &TrayManager::showMainWindowRequested);

    QAction *quickAction = m_trayMenu->addAction(tr("⚡ 快速录入"));
    connect(quickAction, &QAction::triggered, this, &TrayManager::quickEntryRequested);

    m_trayMenu->addSeparator();

    // Desktop mode action
    m_desktopModeAction = m_trayMenu->addAction(tr("🖥 切换到桌面模式"));
    connect(m_desktopModeAction, &QAction::triggered, this, &TrayManager::toggleDesktopModeRequested);

    // Sticky notes submenu
    m_stickySubmenu = m_trayMenu->addMenu(tr("🗒 桌面便签"));

    m_trayMenu->addSeparator();

    QAction *quitAction = m_trayMenu->addAction(tr("退出"));
    connect(quitAction, &QAction::triggered, this, &TrayManager::quitRequested);

    m_trayIcon->setContextMenu(m_trayMenu);

    startReminderTimer();
}

void TrayManager::startReminderTimer()
{
    m_reminderTimer = new QTimer(this);
    connect(m_reminderTimer, &QTimer::timeout, this, &TrayManager::checkTodoReminders);
    m_reminderTimer->start(300000);
}

void TrayManager::checkTodoReminders()
{
    auto *app = ShorthandApplication::instance();
    if (!app) return;

    int overdue = app->todoManager()->getOverdueTodos().size();
    int pending = app->todoManager()->pendingCount();

    if (overdue > 0) {
        showMessage(tr("待办提醒"),
                    tr("你有 %1 个逾期待办，共 %2 个待办事项").arg(overdue).arg(pending + overdue));
    }
}

void TrayManager::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        emit showMainWindowRequested();
    }
}
