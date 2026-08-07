// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktopmodemanager.h"
#include "stickynotecard.h"
#include "core/notemanager.h"
#include "storage/notestorage.h"
#include "storage/database.h"
#include "application/shorthandapplication.h"

#include <QDebug>
#include <QSettings>
#include <QScreen>
#include <QGuiApplication>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QWindow>
#include <QTimer>

// Qt6 native interface for X11 connection
#include <QtGui/qguiapplication_platform.h>
// XCB native window property helpers
#include <xcb/xcb.h>

DesktopModeManager::DesktopModeManager(NoteManager *noteManager, QObject *parent)
    : QObject(parent)
    , m_noteManager(noteManager)
{
    // Load settings
    QSettings settings;
    m_maxNotes = settings.value("desktop/max_notes", 6).toInt();
    m_opacity = settings.value("desktop/opacity", 90).toInt();
    m_defaultColor = settings.value("desktop/default_color", "#409EFF").toString();
    m_continuousAdd = settings.value("desktop/continuous_add", false).toBool();
    m_startInDesktopMode = settings.value("desktop/start_in_desktop_mode", false).toBool();
}

DesktopModeManager::~DesktopModeManager()
{
    if (m_desktopMode) {
        saveGeometry();
    }
    // Clean up cards
    for (auto *card : m_stickyCards) {
        card->close();
        card->deleteLater();
    }
    m_stickyCards.clear();
}

bool DesktopModeManager::isX11() const
{
    return QGuiApplication::platformName() == "xcb";
}

// X11: 设置 _NET_WM_STATE_BELOW 让窗口始终置底，并设置 _NET_WM_STATE_SKIP_TASKBAR
static void setupX11WindowHints(QWidget *w)
{
    if (!w || !w->windowHandle()) return;

    auto *x11App = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11App) return;
    xcb_connection_t *conn = x11App->connection();
    if (!conn) return;

    xcb_window_t xid = static_cast<xcb_window_t>(w->winId());

    // 获取所需 X11 原子
    xcb_intern_atom_cookie_t stateCookie = xcb_intern_atom(conn, false, 12, "_NET_WM_STATE");
    xcb_intern_atom_cookie_t belowCookie = xcb_intern_atom(conn, false, 18, "_NET_WM_STATE_BELOW");
    xcb_intern_atom_cookie_t skipCookie = xcb_intern_atom(conn, false, 20, "_NET_WM_STATE_SKIP_TASKBAR");
    xcb_intern_atom_cookie_t typeCookie = xcb_intern_atom(conn, false, 22, "_NET_WM_WINDOW_TYPE");
    xcb_intern_atom_cookie_t utilCookie = xcb_intern_atom(conn, false, 25, "_NET_WM_WINDOW_TYPE_UTILITY");

    xcb_intern_atom_reply_t *stateAtom = xcb_intern_atom_reply(conn, stateCookie, nullptr);
    xcb_intern_atom_reply_t *belowAtom = xcb_intern_atom_reply(conn, belowCookie, nullptr);
    xcb_intern_atom_reply_t *skipAtom = xcb_intern_atom_reply(conn, skipCookie, nullptr);
    xcb_intern_atom_reply_t *typeAtom = xcb_intern_atom_reply(conn, typeCookie, nullptr);
    xcb_intern_atom_reply_t *utilAtom = xcb_intern_atom_reply(conn, utilCookie, nullptr);

    if (stateAtom && belowAtom && skipAtom) {
        // 设置 _NET_WM_STATE: _NET_WM_STATE_BELOW | _NET_WM_STATE_SKIP_TASKBAR
        xcb_atom_t atoms[2] = { belowAtom->atom, skipAtom->atom };
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, xid,
                            stateAtom->atom, XCB_ATOM_ATOM, 32, 2, atoms);
        xcb_flush(conn);
    }

    if (typeAtom && utilAtom) {
        // 设置窗口类型为 _NET_WM_WINDOW_TYPE_UTILITY
        // 让 WM 知道这是辅助窗口，不会抢焦点
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, xid,
                            typeAtom->atom, XCB_ATOM_ATOM, 32, 1, &utilAtom->atom);
        xcb_flush(conn);
    }

    free(stateAtom);
    free(belowAtom);
    free(skipAtom);
    free(typeAtom);
    free(utilAtom);
}

void DesktopModeManager::applyDesktopWindowHints(QWidget *w)
{
    if (!w) return;

    // 统一使用 FramelessWindowHint + WindowStaysOnBottomHint + Tool + WindowSkipTaskbarHint
    // 不再使用 X11BypassWindowManagerHint（会导致窗口无法被窗口管理器管理）
    w->setWindowFlags(
        Qt::FramelessWindowHint
        | Qt::WindowStaysOnBottomHint
        | Qt::Tool

    );
    w->setAttribute(Qt::WA_TranslucentBackground);
    w->setAttribute(Qt::WA_ShowWithoutActivating);
    w->setAttribute(Qt::WA_X11DoNotAcceptFocus);

    // X11: 延迟到窗口实际创建后通过原生 XCB 设置 _NET_WM_STATE
    // 确保窗口始终置底并跳过任务栏
    if (isX11()) {
        QTimer::singleShot(0, this, [w]() {
            setupX11WindowHints(w);
        });
    }
}

void DesktopModeManager::enterDesktopMode()
{
    if (m_desktopMode) return;
    m_desktopMode = true;

    // 首次进入时从数据库加载已有便签配置
    // 非首次进入时，复用之前已创建的卡片对象（通过 exitDesktopMode 隐藏后重新显示）
    if (m_stickyCards.isEmpty()) {
        loadGeometry();
    }

    // 显示所有便签卡片（包括从数据库加载和新创建的）
    for (auto *card : m_stickyCards.values()) {
        card->fadeIn(200);
    }

    // 如果没有便签，自动置顶最近一条笔记，避免进入便签模式后无任何反馈
    if (m_stickyCards.isEmpty()) {
        QList<NoteData> notes = m_noteManager->getAllNotes();
        if (!notes.isEmpty()) {
            addStickyNote(notes.first().id);
        }
    }

    qInfo() << "[DesktopMode] 进入桌面模式，已有便签:" << m_stickyCards.size();
    emit desktopModeEntered();
}

void DesktopModeManager::exitDesktopMode()
{
    if (!m_desktopMode) return;
    m_desktopMode = false;

    // 保存位置/颜色等配置到数据库
    saveGeometry();

    // 淡出动画并隐藏所有卡片（但保留卡片对象以便下次进入时复用）
    for (auto *card : m_stickyCards.values()) {
        card->fadeOut(250);
    }

    qInfo() << "[DesktopMode] 退出桌面模式";
    emit desktopModeExited();
}

void DesktopModeManager::toggleDesktopMode()
{
    if (m_desktopMode) {
        exitDesktopMode();
    } else {
        enterDesktopMode();
    }
}

void DesktopModeManager::addStickyNote(int noteId)
{
    if (m_stickyCards.contains(noteId)) return;

    if (m_stickyCards.size() >= m_maxNotes) {
        qWarning() << "[DesktopMode] 已达最大便签数:" << m_maxNotes;
        return;
    }

    StickyNoteCard *card = createCard(noteId);
    if (card) {
        m_stickyCards[noteId] = card;
        if (m_desktopMode) {
            card->fadeIn(200);
        }
        emit stickyNoteAdded(noteId);
    }
}

void DesktopModeManager::removeStickyNote(int noteId)
{
    if (!m_stickyCards.contains(noteId)) return;

    StickyNoteCard *card = m_stickyCards.take(noteId);
    if (card) {
        card->fadeOut(150);
        QTimer::singleShot(200, card, &QWidget::deleteLater);
    }

    // 从 sticky_notes 表中删除记录
    auto *app = ShorthandApplication::instance();
    if (app && app->database()) {
        QSqlQuery query(app->database()->connection());
        query.prepare("DELETE FROM sticky_notes WHERE note_id = :id");
        query.bindValue(":id", noteId);
        query.exec();
    }

    emit stickyNoteRemoved(noteId);
}

void DesktopModeManager::pinNoteToDesktop(int noteId)
{
    // 从 QuickEntryDialog "贴到桌面" 调用
    addStickyNote(noteId);

    // 确保 sticky_notes 表中有记录
    auto *app = ShorthandApplication::instance();
    if (app && app->database()) {
        QSqlQuery query(app->database()->connection());
        query.prepare(R"(
            INSERT OR REPLACE INTO sticky_notes
                (note_id, sticky_x, sticky_y, sticky_w, sticky_h, sticky_color, is_visible)
            VALUES (:id, :x, :y, :w, :h, :color, 1)
        )");
        // 默认位置在屏幕右侧，级联偏移
        QScreen *screen = QGuiApplication::primaryScreen();
        int screenW = screen ? screen->availableGeometry().width() : 1920;
        
        int offset = (m_stickyCards.size() % 6) * 30; // Cascade offset
        query.bindValue(":id", noteId);
        query.bindValue(":x", screenW - 300 + offset);
        query.bindValue(":y", 80 + offset);
        query.bindValue(":w", 280);
        query.bindValue(":h", 160);
        query.bindValue(":color", m_defaultColor);
        if (!query.exec()) {
            qWarning() << "插入sticky_notes失败:" << query.lastError().text();
        }
    }
}

void DesktopModeManager::showStickyNotes()
{
    for (auto *card : m_stickyCards.values()) {
        card->show();
    }
}

void DesktopModeManager::hideStickyNotes()
{
    for (auto *card : m_stickyCards.values()) {
        card->hide();
    }
}

void DesktopModeManager::saveGeometry()
{
    auto *app = ShorthandApplication::instance();
    if (!app || !app->database()) return;

    for (auto it = m_stickyCards.begin(); it != m_stickyCards.end(); ++it) {
        StickyNoteCard *card = it.value();
        // 贴边隐藏中的卡片位置在屏幕外，不持久化，避免下次启动恢复成"消失"状态
        if (card->isEdgeDocked()) {
            continue;
        }
        QRect geo = card->geometry();
        QSqlQuery query(app->database()->connection());
        query.prepare(R"(
            UPDATE sticky_notes SET
                sticky_x = :x, sticky_y = :y,
                sticky_w = :w, sticky_h = :h,
                sticky_color = :color,
                is_visible = :vis
            WHERE note_id = :id
        )");
        query.bindValue(":x", geo.x());
        query.bindValue(":y", geo.y());
        query.bindValue(":w", geo.width());
        query.bindValue(":h", geo.height());
        query.bindValue(":color", card->cardColor());
        query.bindValue(":vis", card->isVisible() ? 1 : 0);
        query.bindValue(":id", it.key());
        query.exec();
    }
}

void DesktopModeManager::loadGeometry()
{
    auto *app = ShorthandApplication::instance();
    if (!app || !app->database()) return;

    // 先清理已有卡片（防止重复加载导致内存泄漏）
    for (auto *card : m_stickyCards) {
        card->close();
        card->deleteLater();
    }
    m_stickyCards.clear();

    QSqlQuery query(app->database()->connection());
    query.exec("SELECT * FROM sticky_notes WHERE is_visible = 1 ORDER BY note_id");

    while (query.next()) {
        int noteId = query.value("note_id").toInt();
        int x = query.value("sticky_x").toInt();
        int y = query.value("sticky_y").toInt();
        int w = query.value("sticky_w").toInt();
        int h = query.value("sticky_h").toInt();
        QString color = query.value("sticky_color").toString();

        StickyNoteCard *card = createCard(noteId);
        if (!card) {
            // 笔记已被删除，清理 sticky_notes 表
            QSqlQuery cleanup(app->database()->connection());
            cleanup.prepare("DELETE FROM sticky_notes WHERE note_id = :id");
            cleanup.bindValue(":id", noteId);
            cleanup.exec();
            continue;
        }

        card->setGeometry(x, y, w, h);
        card->setCardColor(color);
        m_stickyCards[noteId] = card;
    }
}

StickyNoteCard *DesktopModeManager::createCard(int noteId)
{
    NoteData note = m_noteManager->getNote(noteId);
    if (note.id <= 0) {
        qWarning() << "[DesktopMode] 笔记不存在:" << noteId;
        return nullptr;
    }

    StickyNoteCard *card = new StickyNoteCard(note);
    applyDesktopWindowHints(card);

    // 笔记内容更新 -> 保存回数据库
    connect(card, &StickyNoteCard::noteUpdated, this, [this](int id, const QString &content) {
        auto *app = ShorthandApplication::instance();
        if (!app) return;
        NoteData d = app->noteManager()->getNote(id);
        if (d.id > 0) {
            d.content = content;
            d.title = content.left(60).section('\n', 0, 0);
            app->noteManager()->updateNote(d);
        }
    });

    // 删除便签 -> 从桌面移除
    connect(card, &StickyNoteCard::deleteRequested, this, [this](int id) {
        removeStickyNote(id);
    });

    // 双击展开 -> 在主窗口聚焦该笔记（TC13 五轮：不再退出桌面模式导致便签消失）
    connect(card, &StickyNoteCard::expandRequested, this, [this](int id) {
        // 发射 stickyNoteRemoved 让 MainWindow 聚焦该笔记，但不退出桌面模式、不隐藏便签
        Q_EMIT stickyNoteRemoved(id);
    });

    // 拖拽/缩放后自动保存几何信息（通过 mouseReleaseEvent 触发）
    connect(card, &StickyNoteCard::geometryChanged, this, [this](int) {
        if (m_desktopMode) {
            saveGeometry();
        }
    });

    return card;
}
