// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QSettings>
#include <QTimer>
#include <DWidgetUtil>
#include "application/shorthandapplication.h"
#include "ui/mainwindow/mainwindow.h"

DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    ShorthandApplication app(argc, argv);
    app.setOrganizationName("deepin");
    app.setApplicationName("UOS速记");
    app.setApplicationDisplayName(QObject::tr("UOS速记"));
    app.setApplicationVersion(APP_VERSION);
    app.setProductName(QObject::tr("UOS速记"));
    app.setApplicationDescription(QObject::tr("轻量级办公信息中枢 — 笔记、待办、会议速记"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("UOS Shorthand - Note, Todo & Meeting Assistant"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    if (!app.initialize()) {
        qCritical("应用初始化失败，退出");
        return 1;
    }
    qInfo() << "[main] 初始化完成，开始创建主窗口";

    MainWindow *window = new MainWindow();
    qInfo() << "[main] 主窗口创建完成";
    window->setMinimumSize(960, 640);
    window->resize(1200, 760);

    QSettings settings;
    bool compactStart = settings.value("startup/compact_mode", false).toBool();
    if (compactStart) {
        window->showMinimized();
        QTimer::singleShot(500, window, [window]() {
            window->onShowQuickEntry();
        });
    } else {
        window->show();
        Dtk::Widget::moveToCenter(window);
        // 启动后自动加载笔记列表
        QTimer::singleShot(200, window, [window]() {
            qInfo() << "[main] 加载初始笔记";
            window->loadInitialNotes();
            qInfo() << "[main] 初始笔记加载完成";
        });
    }
    qInfo() << "[main] 进入事件循环";
    return app.exec();
}