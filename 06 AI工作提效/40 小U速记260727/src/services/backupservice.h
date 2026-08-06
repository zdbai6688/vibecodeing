// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BACKUPSERVICE_H
#define BACKUPSERVICE_H

#include <QObject>
#include <QString>

class Database;

// 数据备份/恢复服务（v1.2 V2-T5 / remaining-work #24）
// 一键备份 SQLite 数据库到用户指定路径，支持从备份恢复；备份文件含时间戳 + 校验和。
class BackupService : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(BackupService)

public:
    struct BackupResult {
        bool ok = false;
        QString message;
        QString backupPath; // 成功时的备份文件完整路径
    };

    explicit BackupService(Database *db, QObject *parent = nullptr);

    // 备份当前数据库到 targetDir（用户选择目录）。文件名自动带时间戳 uos-shorthand-backup-<yyyyMMdd-hhmmss>.db
    // 备份前先做一次 WAL checkpoint，确保数据落盘；校验和写入同目录 .sha256 文件。
    BackupResult backupTo(const QString &targetDir);

    // 从备份文件恢复：目标库文件会先备份为 .pre-restore-<时间戳>，再复制备份内容。
    // 恢复需要重启应用才能生效，调用方负责提示。
    BackupResult restoreFrom(const QString &backupFilePath);

    // 校验备份文件完整性（比对 .sha256）
    bool verifyBackup(const QString &backupFilePath) const;

    static QString defaultBackupDir();

private:
    static QString sha256OfFile(const QString &filePath);

    Database *m_db;
};

#endif // BACKUPSERVICE_H
