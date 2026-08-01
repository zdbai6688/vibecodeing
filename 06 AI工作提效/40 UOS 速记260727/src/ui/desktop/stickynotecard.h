// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STICKYNOTECARD_H
#define STICKYNOTECARD_H

#include <QWidget>
#include <QPoint>
#include <QString>

class QLabel;
class QTextEdit;
class QPushButton;

/**
 * @brief 桌面便签卡片
 *
 * 无边框、置顶的悬浮小窗，展示笔记标题与内容，可拖动、
 * 可编辑，右上角提供关闭按钮。
 */
class StickyNoteCard : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(StickyNoteCard)

public:
    explicit StickyNoteCard(int noteId, const QString &title, const QString &content,
                            const QString &color = "#409EFF", QWidget *parent = nullptr);
    ~StickyNoteCard() override;

    int noteId() const { return m_noteId; }
    QPoint cardPosition() const { return pos(); }
    void setCardPosition(const QPoint &p);
    void showCard();
    void saveContent();

signals:
    void closeRequested(int noteId);
    void moved(int noteId, const QPoint &pos);
    void saved(int noteId, const QString &content);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    int m_noteId;
    QString m_title;
    QPoint m_dragOffset;
    bool m_dragging = false;

    QLabel *m_titleLabel;
    QTextEdit *m_contentEdit;
    QPushButton *m_closeBtn;
};

#endif // STICKYNOTECARD_H
