// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GLOBALSHORTCUTMANAGER_H
#define GLOBALSHORTCUTMANAGER_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QHash>

/**
 * @brief 全局快捷键管理器
 *
 * 通过 X11/XCB GrabKey 机制注册全局热键，使快捷键在应用失去焦点时仍生效。
 * 在 Wayland 环境下回退为普通 QShortcut 行为。
 */
class GlobalShortcutManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_DISABLE_COPY(GlobalShortcutManager)

public:
    explicit GlobalShortcutManager(QObject *parent = nullptr);
    ~GlobalShortcutManager() override;

    /// 注册一个全局快捷键，返回是否注册成功
    bool registerShortcut(const QKeySequence &key, quint32 shortcutId);

    /// 注销指定快捷键
    void unregisterShortcut(quint32 shortcutId);

    /// 注销所有已注册的快捷键
    void unregisterAll();

    /// 检查当前是否运行在 X11 下（全局热键仅 X11 生效）
    bool isX11Platform() const { return m_isX11; }

signals:
    /// 快捷键被触发
    void shortcutActivated(quint32 shortcutId);

protected:
    // QAbstractNativeEventFilter
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#else
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
#endif

private:
    bool initX11();
    bool nativeKeyToKeycode(const QKeySequence &key, unsigned int &keycode, unsigned int &modifiers);
    void cleanupX11();

    bool m_isX11 = false;
    void *m_display = nullptr;
    void *m_rootWindow = nullptr;
    QHash<quint32, QKeySequence> m_registered;
};

#endif // GLOBALSHORTCUTMANAGER_H
