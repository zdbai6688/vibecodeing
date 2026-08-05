// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOPMODEMANAGER_H
#define DESKTOPMODEMANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QJsonObject>

class StickyNoteCard;
class NoteManager;

class DesktopModeManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DesktopModeManager)

public:
    explicit DesktopModeManager(NoteManager *noteManager, QObject *parent = nullptr);
    ~DesktopModeManager() override;

    bool isDesktopMode() const { return m_desktopMode; }

    // Desktop mode lifecycle
    void enterDesktopMode();
    void exitDesktopMode();
    void toggleDesktopMode();

    // Sticky note management
    void addStickyNote(int noteId);
    void removeStickyNote(int noteId);
    void pinNoteToDesktop(int noteId);
    void showStickyNotes();
    void hideStickyNotes();

    // Persistence
    void saveGeometry();
    void loadGeometry();

    // Settings
    int maxStickyNotes() const { return m_maxNotes; }
    void setMaxStickyNotes(int max) { m_maxNotes = qBound(1, max, 12); }
    int stickyOpacity() const { return m_opacity; }
    void setStickyOpacity(int percent) { m_opacity = qBound(40, percent, 95); }
    QString defaultColor() const { return m_defaultColor; }
    void setDefaultColor(const QString &color) { m_defaultColor = color; }
    bool continuousAdd() const { return m_continuousAdd; }
    void setContinuousAdd(bool on) { m_continuousAdd = on; }
    bool startInDesktopMode() const { return m_startInDesktopMode; }
    void setStartInDesktopMode(bool on) { m_startInDesktopMode = on; }

    QList<int> stickyNoteIds() const { return m_stickyCards.keys(); }

signals:
    void desktopModeEntered();
    void desktopModeExited();
    void stickyNoteAdded(int noteId);
    void stickyNoteRemoved(int noteId);

private:
    StickyNoteCard *createCard(int noteId);
    bool isX11() const;
    void applyDesktopWindowHints(QWidget *w);

    NoteManager *m_noteManager;
    QMap<int, StickyNoteCard *> m_stickyCards;
    bool m_desktopMode = false;

    // Settings
    int m_maxNotes = 6;
    int m_opacity = 90;
    QString m_defaultColor = "#409EFF";
    bool m_continuousAdd = false;
    bool m_startInDesktopMode = false;
};

#endif // DESKTOPMODEMANAGER_H
