// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TRAYMANAGER_H
#define TRAYMANAGER_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTimer>
#include <QAction>

class TrayManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(TrayManager)

public:
    explicit TrayManager(QObject *parent = nullptr);
    ~TrayManager() override;

    bool isAvailable() const;
    void showMessage(const QString &title, const QString &message);

signals:
    void showMainWindowRequested();
    void quickEntryRequested();
    void quitRequested();

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void checkTodoReminders();

private:
    void init();
    void initMenu();
    void startReminderTimer();

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QTimer *m_reminderTimer = nullptr;
};

#endif // TRAYMANAGER_H