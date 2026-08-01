// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SHORTHANDAPPLICATION_H
#define SHORTHANDAPPLICATION_H

#include <DApplication>
#include <QSystemTrayIcon>

class Database;
class NoteManager;
class TodoManager;
class TagManager;
class MeetingManager;
class TrayManager;
class Migration;
class AiServiceManager;
class AsrServiceManager;
class ExportService;
class GlobalShortcutManager;

DWIDGET_USE_NAMESPACE

class ShorthandApplication : public DApplication
{
    Q_OBJECT
    Q_DISABLE_COPY(ShorthandApplication)

public:
    explicit ShorthandApplication(int &argc, char **argv);
    ~ShorthandApplication() override;

    static ShorthandApplication *instance();

    // 全局服务
    Database *database() const { return m_database; }
    NoteManager *noteManager() const { return m_noteManager; }
    TodoManager *todoManager() const { return m_todoManager; }
    TagManager *tagManager() const { return m_tagManager; }
    MeetingManager *meetingManager() const { return m_meetingManager; }
    TrayManager *trayManager() const { return m_trayManager; }
    Migration *migration() const { return m_migration; }
    AiServiceManager *aiService() const { return m_aiService; }
    ExportService *exportService() const { return m_exportService; }
    AsrServiceManager *asrService() const { return m_asrService; }
    GlobalShortcutManager *globalShortcut() const { return m_globalShortcut; }

    // 初始化
    bool initialize();

private:
    void initServices();
    void cleanupServices();

    Database *m_database = nullptr;
    NoteManager *m_noteManager = nullptr;
    TodoManager *m_todoManager = nullptr;
    TagManager *m_tagManager = nullptr;
    MeetingManager *m_meetingManager = nullptr;
    TrayManager *m_trayManager = nullptr;
    Migration *m_migration = nullptr;
    AiServiceManager *m_aiService = nullptr;
    AsrServiceManager *m_asrService = nullptr;
    ExportService *m_exportService = nullptr;
    GlobalShortcutManager *m_globalShortcut = nullptr;
};

#endif // SHORTHANDAPPLICATION_H
