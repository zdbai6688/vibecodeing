// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "backupservice.h"
#include "storage/database.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDebug>

BackupService::BackupService(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

QString BackupService::defaultBackupDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/UOS速记/备份";
}

BackupService::BackupResult BackupService::backupTo(const QString &targetDir)
{
    BackupResult result;
    if (!m_db) {
        result.message = tr("数据库服务未初始化");
        return result;
    }

    // WAL 模式下必须先将 WAL 中的内容 checkpoint 回主库文件，否则备份可能不完整
    QSqlQuery checkpoint(m_db->connection());
    if (!checkpoint.exec("PRAGMA wal_checkpoint(FULL)")) {
        qWarning() << "WAL checkpoint 失败:" << checkpoint.lastError().text();
    }

    QDir dir(targetDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        result.message = tr("无法创建备份目录：%1").arg(targetDir);
        return result;
    }

    const QString dbPath = m_db->dataPath() + "/uos-shorthand.db";
    if (!QFile::exists(dbPath)) {
        result.message = tr("未找到数据库文件：%1").arg(dbPath);
        return result;
    }

    const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss");
    const QString backupPath = dir.filePath(QString("uos-shorthand-backup-%1.db").arg(ts));

    // 用 QFile::copy 保持数据库文件字节一致（含 WAL checkpoint 后的完整数据）
    if (!QFile::copy(dbPath, backupPath)) {
        result.message = tr("备份失败：无法复制数据库文件");
        return result;
    }

    // 写入校验和（.sha256 文本文件）
    const QString hash = sha256OfFile(backupPath);
    QFile shaFile(backupPath + ".sha256");
    if (shaFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        shaFile.write((hash + "  " + QFileInfo(backupPath).fileName() + "\n").toUtf8());
        shaFile.close();
    }

    result.ok = true;
    result.backupPath = backupPath;
    result.message = tr("备份成功：%1").arg(QFileInfo(backupPath).fileName());
    qInfo() << "[Backup] 已备份到" << backupPath << "sha256" << hash.left(16) << "...";
    return result;
}

BackupService::BackupResult BackupService::restoreFrom(const QString &backupFilePath)
{
    BackupResult result;
    if (!m_db) {
        result.message = tr("数据库服务未初始化");
        return result;
    }
    if (!QFile::exists(backupFilePath)) {
        result.message = tr("备份文件不存在：%1").arg(backupFilePath);
        return result;
    }
    if (!verifyBackup(backupFilePath)) {
        result.message = tr("备份文件校验失败，文件可能已损坏，已取消恢复");
        return result;
    }

    const QString dbPath = m_db->dataPath() + "/uos-shorthand.db";
    if (!QFile::exists(dbPath)) {
        result.message = tr("未找到目标数据库文件");
        return result;
    }

    // 先关闭连接，避免文件占用导致恢复失败
    m_db->closeConnection();

    // 恢复前先把当前库备份为 .pre-restore-<时间戳>
    const QString preRestore = dbPath + ".pre-restore-" +
            QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss");
    QFile::copy(dbPath, preRestore);

    bool ok = QFile::remove(dbPath);
    if (ok) {
        ok = QFile::copy(backupFilePath, dbPath);
    }

    // 重新打开数据库
    if (!m_db->initialize()) {
        // 恢复失败：尝试用恢复前备份回滚
        qWarning() << "[Backup] 恢复后数据库无法打开，尝试回滚";
        QFile::remove(dbPath);
        QFile::copy(preRestore, dbPath);
        m_db->initialize();
        result.message = tr("恢复失败：数据库无法打开，已自动回滚");
        return result;
    }

    result.ok = ok;
    result.message = ok
        ? tr("恢复成功！数据已从备份恢复，建议重启应用以刷新界面。")
        : tr("恢复失败：无法写入数据库文件");
    qInfo() << "[Backup] 已从备份恢复，原库备份为" << preRestore;
    return result;
}

bool BackupService::verifyBackup(const QString &backupFilePath) const
{
    // 优先比对 .sha256；若校验文件缺失则回退为「文件存在且非空」
    const QString shaFile = backupFilePath + ".sha256";
    if (QFile::exists(shaFile)) {
        QFile f(shaFile);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString line = QString::fromUtf8(f.readAll()).trimmed();
            f.close();
            const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (!parts.isEmpty()) {
                return sha256OfFile(backupFilePath) == parts[0];
            }
        }
        return false;
    }
    QFileInfo fi(backupFilePath);
    return fi.exists() && fi.size() > 0;
}

QString BackupService::sha256OfFile(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&f)) {
        return QString::fromLatin1(hash.result().toHex());
    }
    return QString();
}
