#include "screenshotmanager.h"

#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <QTimer>

ScreenshotManager::ScreenshotManager(QObject *parent)
    : QObject(parent)
{
}

QString ScreenshotManager::defaultScreenshotDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/screenshots";
    QDir().mkpath(dir);
    return dir;
}

QString ScreenshotManager::generateFileName() const
{
    return defaultScreenshotDir() + "/screenshot_" +
           QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";
}

void ScreenshotManager::captureScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        emit screenshotFailed(tr("无法获取屏幕"));
        return;
    }

    QPixmap pixmap = screen->grabWindow(0);
    savePixmap(pixmap);
}

void ScreenshotManager::captureRegion()
{
    QProcess *proc = new QProcess(this);
    QStringList args;
    args << "-s" << "-f" << generateFileName();

    QStringList tools = {"deepin-screen-recorder", "gnome-screenshot", "spectacle", "xfce4-screenshooter"};
    QString chosenTool;

    for (const auto &tool : tools) {
        QProcess which;
        which.start("which", {tool});
        which.waitForFinished(2000);
        if (which.exitCode() == 0) {
            chosenTool = tool;
            break;
        }
    }

    if (chosenTool.isEmpty()) {
        // 降级：全屏截图
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            QPixmap pixmap = screen->grabWindow(0);
            savePixmap(pixmap);
        }
        return;
    }

    QString savePath = generateFileName();
    if (chosenTool == "deepin-screen-recorder") {
        proc->start(chosenTool, {"-s", "-f", savePath});
    } else if (chosenTool == "gnome-screenshot") {
        proc->start(chosenTool, {"-a", "-f", savePath});
    } else {
        proc->start(chosenTool, {"-s", "-o", savePath});
    }

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, savePath](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        if (exitCode == 0 && QFile::exists(savePath)) {
            emit screenshotTaken(savePath);
        } else {
            // 降级：检查剪贴板
            const QClipboard *clipboard = QApplication::clipboard();
            if (!clipboard->pixmap().isNull()) {
                savePixmap(clipboard->pixmap());
            } else {
                emit screenshotFailed(tr("截图取消或失败"));
            }
        }
    });
}

void ScreenshotManager::savePixmap(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        emit screenshotFailed(tr("截图内容为空"));
        return;
    }

    QString filePath = generateFileName();
    if (pixmap.save(filePath, "PNG")) {
        emit screenshotTaken(filePath);
    } else {
        emit screenshotFailed(tr("保存截图失败"));
    }
}