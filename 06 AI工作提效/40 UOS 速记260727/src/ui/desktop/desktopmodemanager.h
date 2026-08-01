// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOPMODEMANAGER_H
#define DESKTOPMODEMANAGER_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QPoint>
#include <QString>
#include <QColor>

class NoteManager;
class StickyNoteCard;

/**
 * @brief 桌面便签模式管理器
 *
 * 负责「便签模式」：将笔记以桌面便签卡片形式悬浮展示在桌面上，
 * 支持从快速录入「贴到桌面」、托盘菜单进入/退出桌面模式。
 */
class DesktopModeManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DesktopModeManager)

public:
    explicit DesktopModeManager(NoteManager *noteManager, QObject *parent = nullptr);
    ~DesktopModeManager() override;

    bool isDesktopMode() const { return m_desktopMode; }
    void enterDesktopMode();
    void exitDesktopMode();

    /// 当前固定在桌面的笔记 ID 列表
    QList<int> stickyNoteIds() const;
    /// 将笔记固定到桌面（创建便签卡片）
    void pinNoteToDesktop(int noteId);

signals:
    void desktopModeEntered();
    void desktopModeExited();

private slots:
    void onCardCloseRequested(int noteId);
    void onCardMoved(int noteId, const QPoint &pos);
    void onCardSaved(int noteId, const QString &content);

private:
    void loadPinnedNotes();
    StickyNoteCard *createCard(int noteId);
    void clearCards();
    void saveCardPosition(int noteId, const QPoint &pos);
    QPoint loadCardPosition(int noteId) const;
    QColor loadDefaultColor() const;

    NoteManager *m_noteManager;
    QHash<int, StickyNoteCard *> m_cards;
    bool m_desktopMode = false;
};

#endif // DESKTOPMODEMANAGER_H
