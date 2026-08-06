// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "globalshortcutmanager.h"

#include <QDebug>
#include <QGuiApplication>
#include <QKeySequence>
#include <QHash>

#if defined(Q_OS_LINUX) && defined(QT_GUI_LIB)
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

// NumLockMask 在某些 X11 环境中未定义，使用 Mod2Mask
#ifndef NumLockMask
#define NumLockMask Mod2Mask
#endif
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

static unsigned int qtModsToX11Mods(int qtMods)
{
    unsigned int mods = 0;
    if (qtMods & Qt::ShiftModifier)   mods |= ShiftMask;
    if (qtMods & Qt::ControlModifier) mods |= ControlMask;
    if (qtMods & Qt::AltModifier)     mods |= Mod1Mask;
    if (qtMods & Qt::MetaModifier)    mods |= Mod4Mask;
    return mods;
}
#endif

// ─── XGrabKey 失败检测 ─────────────────────────────────────────────
// XGrabKey 的错误是异步返回的（BadAccess = 与其他客户端/WM 抢键冲突）。
// 这里用临时 error handler + XSync 同步捕获，注册失败时返回 false，
// 让上层走应用内快捷键回退，而不是“假装注册成功”。
#if defined(Q_OS_LINUX)
static int s_grabErrorCode = 0;
static int grabKeyErrorHandler(Display *, XErrorEvent *e)
{
    s_grabErrorCode = e->error_code;
    return 0;
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
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        qWarning() << "GlobalShortcutManager: 无法打开 X11 Display";
        m_isX11 = false;
        return false;
    }
    m_rootWindow = (void*)(long)DefaultRootWindow((Display*)m_display);
    qInfo() << "GlobalShortcutManager: X11 全局热键初始化成功";
    return true;
#else
    return false;
#endif
}

void GlobalShortcutManager::cleanupX11()
{
#if defined(Q_OS_LINUX)
    if (m_display) {
        XCloseDisplay((Display*)m_display);
        m_display = nullptr;
    }
#endif
}

// ─── 注册 / 注销 ───────────────────────────────────────────────────
bool GlobalShortcutManager::registerShortcut(const QKeySequence &key, quint32 shortcutId)
{
#if defined(Q_OS_LINUX)
    if (!m_isX11 || !m_display) {
        qWarning() << "GlobalShortcutManager: 无法注册热键 - 非 X11 环境";
        return false;
    }

    Display *display = (Display*)m_display;
    Window root = (Window)(long)m_rootWindow;

    unsigned int keycode = 0;
    unsigned int modifiers = 0;
    if (!nativeKeyToKeycode(key, keycode, modifiers)) {
        qWarning() << "GlobalShortcutManager: 无法解析快捷键" << key.toString();
        return false;
    }

    // 使用 XGrabKey 注册全局热键
    // 需要同时注册 IgnoreMods（CapsLock / NumLock 等不影响触发）
    const unsigned int ignoreMods[] = {
        0,
        LockMask,
        Mod2Mask,
        Mod2Mask | LockMask,
        NumLockMask ? NumLockMask : 0,
    };

    s_grabErrorCode = 0;
    XErrorHandler oldHandler = XSetErrorHandler(grabKeyErrorHandler);
    for (unsigned int ignore : ignoreMods) {
        XGrabKey(display, keycode, modifiers | ignore, root, True,
                 GrabModeAsync, GrabModeAsync);
    }
    XSync(display, False);
    XSetErrorHandler(oldHandler);

    if (s_grabErrorCode != 0) {
        qWarning() << "GlobalShortcutManager: 全局热键注册被拒绝(错误码" << s_grabErrorCode
                   << "，可能与其他应用/WM 冲突):" << key.toString();
        // 回滚可能已部分成功的 grab
        for (unsigned int ignore : ignoreMods) {
            XUngrabKey(display, keycode, modifiers | ignore, root);
        }
        XSync(display, False);
        return false;
    }

    m_registered.insert(shortcutId, key);
    qInfo() << "GlobalShortcutManager: 已注册全局热键" << key.toString()
            << "id=" << shortcutId;

    // 安装 native event filter
    if (m_registered.size() == 1) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }

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

    Display *display = (Display*)m_display;
    Window root = (Window)(long)m_rootWindow;

    auto key = m_registered.value(shortcutId);
    unsigned int keycode = 0;
    unsigned int modifiers = 0;
    if (nativeKeyToKeycode(key, keycode, modifiers)) {
        const unsigned int ignoreMods[] = {
            0, LockMask, Mod2Mask, Mod2Mask | LockMask, NumLockMask ? NumLockMask : 0
        };
        for (unsigned int ignore : ignoreMods) {
            XUngrabKey(display, keycode, modifiers | ignore, root);
        }
        XSync(display, False);
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
    if (!m_display) return false;

    Display *display = (Display*)m_display;
    int qtKey = key[0].key();
    int qtMods = key[0].keyboardModifiers();

    KeySym ks = qtKeyToX11KeySym(qtKey);
    if (ks == NoSymbol) {
        // 尝试用 Qt 内部的字符转换
        ks = XK_space;  // fallback
        qWarning() << "GlobalShortcutManager: 无法转换 Qt key" << qtKey;
        return false;
    }

    KeyCode kc = XKeysymToKeycode(display, ks);
    if (kc == 0) {
        qWarning() << "GlobalShortcutManager: 无法获取 KeyCode 对于 KeySym" << ks;
        return false;
    }

    keycode = (unsigned int)kc;
    modifiers = qtModsToX11Mods(qtMods);
    return true;
#else
    Q_UNUSED(key);
    Q_UNUSED(keycode);
    Q_UNUSED(modifiers);
    return false;
#endif
}

// ─── Native Event Filter ───────────────────────────────────────────
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

    // 使用 X11 事件处理
    XEvent *xevent = static_cast<XEvent*>(message);
    if (xevent->type != KeyPress) {
        return false;
    }

    // 检查是否是我们要处理的 KeyPress
    KeyCode pressedKc = xevent->xkey.keycode;
    unsigned int pressedMods = xevent->xkey.state & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);

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
