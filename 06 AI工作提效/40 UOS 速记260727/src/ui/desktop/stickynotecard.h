// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STICKYNOTECARD_H
#define STICKYNOTECARD_H

#include <QWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QPoint>
#include "storage/notestorage.h"

class StickyNoteCard : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(StickyNoteCard)

public:
    explicit StickyNoteCard(const NoteData &note, QWidget *parent = nullptr);
    ~StickyNoteCard() override;

    void setCardColor(const QString &color);
    QString cardColor() const { return m_cardColor; }
    int noteId() const { return m_noteData.id; }

    // Animations
    void fadeIn(int ms = 200);
    void fadeOut(int ms = 150);

signals:
    void noteUpdated(int noteId, const QString &content);
    void deleteRequested(int noteId);
    void expandRequested(int noteId);
    void geometryChanged(int noteId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onTextChanged();
    void onDeleteClicked();
    void onColorClicked();
    void onPinToggled();

private:
    void initUI();
    void updateStyleSheet();
    void showToolbar(bool show);
    void setupMenu();

    NoteData m_noteData;
    QString m_cardColor = "#409EFF";
    bool m_isPinned = false;
    bool m_isEditing = false;
    bool m_isDragging = false;
    bool m_isResizing = false;
    QPoint m_dragStartPos;
    QPoint m_dragStartGlobal;
    QSize m_dragStartSize;

    // UI elements
    QLabel *m_timeLabel;
    QTextEdit *m_contentEdit;
    QWidget *m_toolbar;
    QPushButton *m_pinBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_colorBtn;
    QWidget *m_resizeHandle;

    // Debounce timer for auto-save
    QTimer *m_saveTimer;
};

#endif // STICKYNOTECARD_H
