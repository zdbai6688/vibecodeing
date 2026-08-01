// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktopmodemanager.h"
#include "stickynotecard.h"
#include "core/notemanager.h"
#include "application/shorthandapplication.h"
#include "storage/database.h"
#include "storage/notestorage.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSettings>
#include <QDateTime>
#include <QDebug>

DesktopModeManager::DesktopModeManager(NoteManager *noteManager, QObject *parent)
    : QObject(parent)
    , m_noteManager(noteManager)
{
    loadPinnedNotes();
}

DesktopModeManager::~DesktopModeManager()
{
    clearCards();
}

void DesktopModeManager::loadPinnedNotes()
{
    QSqlQuery query(ShorthandApplication::instance()->database()->connection());
    if (query.exec("SELECT note_id FROM sticky_notes WHERE is_visible = 1 ORDER BY note_id")) {
        while (query.next()) {
            int noteId = query.value(0).toInt();
            if (noteId > 0 && !m_cards.contains(noteId)) {
                StickyNoteCard *card = createCard(noteId);
                if (card) m_cards.insert(noteId, card);
            }
        }
    }
}

StickyNoteCard *DesktopModeManager::createCard(int noteId)
{
    NoteData note = m_noteManager->getNote(noteId);
    if (note.id <= 0) return nullptr;

    QSqlQuery query(ShorthandApplication::instance()->database()->connection());
    query.prepare("SELECT sticky_x, sticky_y, sticky_color FROM sticky_notes WHERE note_id = :id");
    query.bindValue(":id", noteId);
    QString color = loadDefaultColor().name();
    QPoint pos(-1, -1);
    if (query.exec() && query.next()) {
        int x = query.value(0).toInt();
        int y = query.value(1).toInt();
        if (x >= 0 && y >= 0) pos = QPoint(x, y);
        QString c = query.value(2).toString();
        if (!c.isEmpty()) color = c;
    }

    QString title = note.title.isEmpty() ? tr("无标题") : note.title;
    StickyNoteCard *card = new StickyNoteCard(noteId, title, note.content, color);
    if (pos.x() >= 0) card->setCardPosition(pos);
    connect(card, &StickyNoteCard::closeRequested, this, &DesktopModeManager::onCardCloseRequested);
    connect(card, &StickyNoteCard::moved, this, &DesktopModeManager::onCardMoved);
    connect(card, &StickyNoteCard::saved, this, &DesktopModeManager::onCardSaved);
    return card;
}

void DesktopModeManager::enterDesktopMode()
{
    if (m_desktopMode) return;
    m_desktopMode = true;

    // 确保已固定笔记都有卡片；将空笔记也纳入桌面便签则不需要
    for (auto it = m_cards.begin(); it != m_cards.end(); ++it) {
        it.value()->showCard();
    }
    emit desktopModeEntered();
}

void DesktopModeManager::exitDesktopMode()
{
    if (!m_desktopMode) return;
    m_desktopMode = false;

    for (auto it = m_cards.begin(); it != m_cards.end(); ++it) {
        it.value()->hide();
    }
    emit desktopModeExited();
}

QList<int> DesktopModeManager::stickyNoteIds() const
{
    return m_cards.keys();
}

void DesktopModeManager::pinNoteToDesktop(int noteId)
{
    NoteData note = m_noteManager->getNote(noteId);
    if (note.id <= 0) return;

    QSqlQuery query(ShorthandApplication::instance()->database()->connection());
    query.prepare("INSERT OR IGNORE INTO sticky_notes (note_id, sticky_x, sticky_y, sticky_w, sticky_h, sticky_color, is_visible) "
                  "VALUES (:id, -1, -1, 280, 160, :color, 1)");
    query.bindValue(":id", noteId);
    query.bindValue(":color", loadDefaultColor().name());
    if (!query.exec()) {
        qWarning() << "固定便签失败:" << query.lastError().text();
    }

    if (m_cards.contains(noteId)) {
        if (m_desktopMode) m_cards.value(noteId)->showCard();
        return;
    }

    StickyNoteCard *card = createCard(noteId);
    if (!card) return;
    m_cards.insert(noteId, card);
    if (m_desktopMode) card->showCard();
}

void DesktopModeManager::onCardCloseRequested(int noteId)
{
    StickyNoteCard *card = m_cards.take(noteId);
    if (card) {
        card->hide();
        card->deleteLater();
    }
    // 取消固定：从 sticky_notes 中删除
    QSqlQuery query(ShorthandApplication::instance()->database()->connection());
    query.prepare("DELETE FROM sticky_notes WHERE note_id = :id");
    query.bindValue(":id", noteId);
    query.exec();
}

void DesktopModeManager::onCardMoved(int noteId, const QPoint &pos)
{
    saveCardPosition(noteId, pos);
}

void DesktopModeManager::onCardSaved(int noteId, const QString &content)
{
    if (!m_noteManager) return;
    NoteData note = m_noteManager->getNote(noteId);
    if (note.id <= 0) return;
    note.content = content;
    note.modificationDatetime = QDateTime::currentSecsSinceEpoch();
    m_noteManager->updateNote(note);
}

void DesktopModeManager::clearCards()
{
    for (auto it = m_cards.begin(); it != m_cards.end(); ++it) {
        it.value()->hide();
        it.value()->deleteLater();
    }
    m_cards.clear();
}

void DesktopModeManager::saveCardPosition(int noteId, const QPoint &pos)
{
    QSqlQuery query(ShorthandApplication::instance()->database()->connection());
    query.prepare("UPDATE sticky_notes SET sticky_x = :x, sticky_y = :y WHERE note_id = :id");
    query.bindValue(":x", pos.x());
    query.bindValue(":y", pos.y());
    query.bindValue(":id", noteId);
    query.exec();
}

QPoint DesktopModeManager::loadCardPosition(int noteId) const
{
    Q_UNUSED(noteId)
    return QPoint(-1, -1);
}

QColor DesktopModeManager::loadDefaultColor() const
{
    QSettings settings;
    return QColor(settings.value("desktop/default_color", "#409EFF").toString());
}
