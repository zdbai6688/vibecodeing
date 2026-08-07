// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "globalshortcutmanager.h"

#include <QDebug>
#include <QGuiApplication>
#include <QKeySequence>
#include <QHash>
#include <QtGui/qguiapplication_platform.h>

#if defined(Q_OS_LINUX) && defined(QT_GUI_LIB)
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#endif

// ─── 工具：将 Qt::Key 转换为 X11 KeySym ─────────────────────────────
#if defined(Q_OS_LINUX)
static KeySym qtKeyToX11KeySym(int qtKey)
{
    // 字母键
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return XK_A + (qtKey - Qt::Key_A);
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return XK_0 + (qtKey - Qt::Key_0);

    // 功能键
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
        return XK_F1 + (qtKey - Qt::Key_F1);

    // 特殊键
    switch (qtKey) {
    case Qt::Key_Space:         return XK_space;
    case Qt::Key_Escape:        return XK_Escape;
    case Qt::Key_Return:        return XK_Return;
    case Qt::Key_Enter:         return XK_KP_Enter;
    case Qt::Key_Tab:           return XK_Tab;
    case Qt::Key_Backspace:     return XK_BackSpace;
    case Qt::Key_Delete:        return XK_Delete;
    case Qt::Key_Home:          return XK_Home;
    case Qt::Key_End:           return XK_End;
    case Qt::Key_PageUp:        return XK_Page_Up;
    case Qt::Key_PageDown:      return XK_Page_Down;
    case Qt::Key_Left:          return XK_Left;
    case Qt::Key_Right:         return XK_Right;
    case Qt::Key_Up:            return XK_Up;
    case Qt::Key_Down:          return XK_Down;
    case Qt::Key_Insert:        return XK_Insert;
    case Qt::Key_Minus:         return XK_minus;
    case Qt::Key_Equal:         return XK_equal;
    case Qt::Key_BracketLeft:   return XK_bracketleft;
    case Qt::Key_BracketRight:  return XK_bracketright;
    case Qt::Key_Semicolon:     return XK_semicolon;
    case Qt::Key_Apostrophe:    return XK_apostrophe;
    case Qt::Key_Comma:         return XK_comma;
    case Qt::Key_Period:        return XK_period;
    case Qt::Key_Slash:         return XK_slash;
    case Qt::Key_Backslash:     return XK_backslash;
    case Qt::Key_QuoteLeft:     return XK_grave;
    default: break;
    }
    return NoSymbol;
}

static unsigned int qtModsToXcbMods(int qtMods)
{
    unsigned int mods = 0;
    if (qtMods & Qt::ShiftModifier)   mods |= XCB_MOD_MASK_SHIFT;
    if (qtMods & Qt::ControlModifier) mods |= XCB_MOD_MASK_CONTROL;
    if (qtMods & Qt::AltModifier)     mods |= XCB_MOD_MASK_1;
    if (qtMods & Qt::MetaModifier)    mods |= XCB_MOD_MASK_4;
    return mods;
}
#endif

// ─── 构造 / 析构 ─────────────────────────────────────────────────────
GlobalShortcutManager::GlobalShortcutManager(QObject *parent)
    : QObject(parent)
{
    // 检查是否是 X11 平台
    m_isX11 = QGuiApplication::platformName().startsWith("xcb", Qt::CaseInsensitive);
    if (m_isX11) {
        initX11();
    } else {
        qInfo() << "GlobalShortcutManager: 非 X11 平台 (" << QGuiApplication::platformName()
                << ")，全局热键不可用，将使用应用内快捷键";
    }
}

GlobalShortcutManager::~GlobalShortcutManager()
{
    unregisterAll();
    cleanupX11();
}

// ─── X11 初始化 ────────────────────────────────────────────────────
bool GlobalShortcutManager::initX11()
{
#if defined(Q_OS_LINUX)
    // 关键：必须使用 Qt 自己的 xcb 连接来 GrabKey，
    // 这样被捕获的按键事件会进入 Qt 事件循环，nativeEventFilter 才能收到。
    // 如果另开 Xlib Display 去 GrabKey，事件会落在那条从未被读取的连接上，快捷键永不生效（TC13 根因）。
    QGuiApplication *app = qobject_cast<QGuiApplication *>(QGuiApplication::instance());
    auto *x11App = app ? app->nativeInterface<QNativeInterface::QX11Application>() : nullptr;
    if (!x11App || !x11App->connection()) {
        qWarning() << "GlobalShortcutManager: 无法获取 Qt xcb 连接";
        m_isX11 = false;
        return false;
    }
    m_connection = reinterpret_cast<void *>(x11App->connection());

    const xcb_setup_t *setup = xcb_get_setup(reinterpret_cast<xcb_connection_t *>(m_connection));
    const xcb_screen_t *screen = nullptr;
    for (xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup); it.rem; xcb_screen_next(&it)) {
        screen = it.data;
        break;
    }
    if (!screen) {
        qWarning() << "GlobalShortcutManager: 无法获取根窗口";
        m_isX11 = false;
        return false;
    }
    m_rootWindow = reinterpret_cast<void *>(static_cast<quintptr>(screen->root));
    qInfo() << "GlobalShortcutManager: xcb 全局热键初始化成功";
    return true;
#else
    return false;
#endif
}

void GlobalShortcutManager::cleanupX11()
{
#if defined(Q_OS_LINUX)
    // m_connection 由 Qt 持有，无需关闭
    m_connection = nullptr;
    m_rootWindow = nullptr;
#endif
}

// ─── 注册 / 注销 ───────────────────────────────────────────────────
bool GlobalShortcutManager::registerShortcut(const QKeySequence &key, quint32 shortcutId)
{
#if defined(Q_OS_LINUX)
    if (!m_isX11 || !m_connection) {
        qWarning() << "GlobalShortcutManager: 无法注册热键 - 非 X11 环境";
        return false;
    }

    xcb_connection_t *conn = reinterpret_cast<xcb_connection_t *>(m_connection);
    xcb_window_t root = static_cast<xcb_window_t>(reinterpret_cast<quintptr>(m_rootWindow));

    unsigned int keycode = 0;
    unsigned int modifiers = 0;
    if (!nativeKeyToKeycode(key, keycode, modifiers)) {
        qWarning() << "GlobalShortcutManager: 无法解析快捷键" << key.toString();
        return false;
    }

    // 使用 xcb_grab_key 注册全局热键。
    // 同时注册忽略修饰键（CapsLock / NumLock 等）的变体，避免锁定键影响触发。
    const unsigned int ignoreMods[] = {
        0,
        XCB_MOD_MASK_LOCK,
        XCB_MOD_MASK_2,
        XCB_MOD_MASK_2 | XCB_MOD_MASK_LOCK,
    };

    bool anyOk = false;
    for (unsigned int ignore : ignoreMods) {
        xcb_void_cookie_t cookie = xcb_grab_key(conn, 0, root, modifiers | ignore, keycode,
                                                XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        xcb_generic_error_t *err = xcb_request_check(conn, cookie);
        if (err) {
            qWarning() << "GlobalShortcutManager: 全局热键注册被拒绝(错误码" << err->error_code
                       << "，可能与其他应用/WM 冲突):" << key.toString();
            free(err);
            continue;
        }
        anyOk = true;
    }

    if (!anyOk) {
        // 回滚可能已部分成功的 grab
        for (unsigned int ignore : ignoreMods) {
            xcb_ungrab_key(conn, keycode, modifiers | ignore, root);
        }
        xcb_flush(conn);
        return false;
    }

    m_registered.insert(shortcutId, key);
    qInfo() << "GlobalShortcutManager: 已注册全局热键" << key.toString()
            << "id=" << shortcutId;

    // 安装 native event filter
    if (m_registered.size() == 1) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
    xcb_flush(conn);

    return true;
#else
    Q_UNUSED(key);
    Q_UNUSED(shortcutId);
    return false;
#endif
}

void GlobalShortcutManager::unregisterShortcut(quint32 shortcutId)
{
#if defined(Q_OS_LINUX)
    if (!m_isX11 || !m_registered.contains(shortcutId)) return;

    xcb_connection_t *conn = reinterpret_cast<xcb_connection_t *>(m_connection);
    xcb_window_t root = static_cast<xcb_window_t>(reinterpret_cast<quintptr>(m_rootWindow));

    auto key = m_registered.value(shortcutId);
    unsigned int keycode = 0;
    unsigned int modifiers = 0;
    if (nativeKeyToKeycode(key, keycode, modifiers)) {
        const unsigned int ignoreMods[] = {
            0, XCB_MOD_MASK_LOCK, XCB_MOD_MASK_2, XCB_MOD_MASK_2 | XCB_MOD_MASK_LOCK
        };
        for (unsigned int ignore : ignoreMods) {
            xcb_ungrab_key(conn, keycode, modifiers | ignore, root);
        }
        xcb_flush(conn);
    }

    m_registered.remove(shortcutId);
    qInfo() << "GlobalShortcutManager: 已注销热键 id=" << shortcutId;

    if (m_registered.isEmpty()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
#else
    Q_UNUSED(shortcutId);
#endif
}

void GlobalShortcutManager::unregisterAll()
{
    QList<quint32> ids = m_registered.keys();
    for (quint32 id : ids) {
        unregisterShortcut(id);
    }
}

// ─── 键序解析 ─────────────────────────────────────────────────────
bool GlobalShortcutManager::nativeKeyToKeycode(const QKeySequence &key,
                                               unsigned int &keycode,
                                               unsigned int &modifiers)
{
#if defined(Q_OS_LINUX)
    if (!m_connection) return false;

    int qtKey = key[0].key();
    int qtMods = key[0].keyboardModifiers();

    KeySym ks = qtKeyToX11KeySym(qtKey);
    if (ks == NoSymbol) {
        qWarning() << "GlobalShortcutManager: 无法转换 Qt key" << qtKey;
        return false;
    }

    // 用临时 Xlib 连接转换 KeySym → KeyCode（KeyCode 是服务器级全局值，任意连接可转）
    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        qWarning() << "GlobalShortcutManager: 无法打开 X11 Display 做键位转换";
        return false;
    }
    KeyCode kc = XKeysymToKeycode(dpy, ks);
    XCloseDisplay(dpy);

    if (kc == 0) {
        qWarning() << "GlobalShortcutManager: 无法获取 KeyCode 对于 KeySym" << ks;
        return false;
    }

    keycode = (unsigned int)kc;
    modifiers = qtModsToXcbMods(qtMods);
    return true;
#else
    Q_UNUSED(key);
    Q_UNUSED(keycode);
    Q_UNUSED(modifiers);
    return false;
#endif
}

// ─── Native Event Filter ───────────────────────────────────────────
// 注意：Qt6 xcb 插件传入的 message 是 `xcb_generic_event_t*`（不是 Xlib XEvent*）。
// 两者内存布局不同，直接当 XEvent 用会读到错位数据导致快捷键永不触发（TC13 根因）。
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool GlobalShortcutManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
#else
bool GlobalShortcutManager::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
#endif
{
    Q_UNUSED(result);

#if defined(Q_OS_LINUX)
    if (eventType != "xcb_generic_event_t" || !message) {
        return false;
    }

    auto *generic = static_cast<xcb_generic_event_t *>(message);
    const uint8_t responseType = generic->response_type & ~0x80; // 去掉发送方标记位
    if (responseType != XCB_KEY_PRESS) {
        return false;
    }

    auto *keyEvent = reinterpret_cast<xcb_key_press_event_t *>(generic);
    const xcb_keycode_t pressedKc = keyEvent->detail;
    const uint16_t state = keyEvent->state;

    // 归一化修饰键（屏蔽 CapsLock=Lock, NumLock=Mod2，仅比对 Shift/Ctrl/Alt/Meta）
    const unsigned int relevant = XCB_MOD_MASK_SHIFT | XCB_MOD_MASK_CONTROL
                                | XCB_MOD_MASK_1 | XCB_MOD_MASK_4;
    const unsigned int pressedMods = state & relevant;

    for (auto it = m_registered.constBegin(); it != m_registered.constEnd(); ++it) {
        unsigned int regKc = 0;
        unsigned int regMods = 0;
        if (nativeKeyToKeycode(it.value(), regKc, regMods)) {
            if (pressedKc == regKc && pressedMods == regMods) {
                qInfo() << "GlobalShortcutManager: 触发全局热键" << it.value().toString();
                emit shortcutActivated(it.key());
                return true;  // 事件已处理，阻止进一步传播
            }
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif

    return false;
}
