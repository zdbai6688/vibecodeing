// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shorthandapplication.h"
#include "storage/database.h"
#include "core/notemanager.h"
#include "core/todomanager.h"
#include "core/tagmanager.h"
#include "core/meetingmanager.h"
#include "ui/tray/traymanager.h"
#include "services/migration.h"
#include "services/aiservice.h"
#include "services/asrservice.h"
#include "services/exportservice.h"
#include "services/backupservice.h"
#include "services/globalshortcutmanager.h"
#include "ui/desktop/desktopmodemanager.h"

#include <QDir>
#include <QStandardPaths>
#include <QDebug>

ShorthandApplication::ShorthandApplication(int &argc, char **argv)
    : DApplication(argc, argv)
{
    setQuitOnLastWindowClosed(false); // 系统托盘模式
}

ShorthandApplication::~ShorthandApplication()
{
    cleanupServices();
}

ShorthandApplication *ShorthandApplication::instance()
{
    return qobject_cast<ShorthandApplication *>(QCoreApplication::instance());
}

bool ShorthandApplication::initialize()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    initServices();

    bool dbOk = m_database->initialize();
    if (!dbOk) {
        qWarning() << "数据库初始化失败";
        return false;
    }

    m_migration = new Migration(m_database, m_noteManager, m_tagManager, this);
    if (m_migration->hasVoiceNoteData()) {
        qInfo() << "检测到deepin-voice-note数据，执行导入...";
        int count = m_migration->importVoiceNoteData();
        qInfo() << "数据导入完成，共" << count << "条";
    }

    qInfo() << "UOS速记初始化完成，数据目录:" << dataDir;
    return true;
}

void ShorthandApplication::initServices()
{
    m_database = new Database(this);

    m_noteManager = new NoteManager(m_database, this);
    m_todoManager = new TodoManager(m_database, this);
    m_tagManager = new TagManager(m_database, this);
    m_meetingManager = new MeetingManager(m_database, this);

    m_aiService = new AiServiceManager(this);
    m_asrService = new AsrServiceManager(this);
    m_exportService = new ExportService(this);
    m_backupService = new BackupService(m_database, this);
    m_trayManager = new TrayManager(this);
    m_globalShortcut = new GlobalShortcutManager(this);
    m_desktopModeManager = new DesktopModeManager(m_noteManager, this);
}

void ShorthandApplication::cleanupServices()
{
    m_globalShortcut = nullptr;
    m_trayManager = nullptr;
    m_meetingManager = nullptr;
    m_aiService = nullptr;
    m_backupService = nullptr;
    m_migration = nullptr;
    m_noteManager = nullptr;
    m_todoManager = nullptr;
    m_tagManager = nullptr;
    m_database = nullptr;
}
