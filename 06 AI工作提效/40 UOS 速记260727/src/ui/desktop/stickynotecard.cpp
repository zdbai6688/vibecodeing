// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "stickynotecard.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QDebug>

StickyNoteCard::StickyNoteCard(int noteId, const QString &title, const QString &content,
                               const QString &color, QWidget *parent)
    : QWidget(parent)
    , m_noteId(noteId)
    , m_title(title)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedSize(280, 200);
    setStyleSheet(QString(R"(
        #stickyCard {
            background: %1;
            border: 1px solid palette(mid);
            border-radius: 8px;
        }
        #stickyTitle {
            color: #FFFFFF;
            font-size: 13px;
            font-weight: 600;
            background: transparent;
            border: none;
            padding: 2px 6px;
        }
        #stickyContent {
            background: rgba(255,255,255,0.85);
            border: none;
            border-radius: 4px;
            font-size: 12px;
            padding: 6px;
        }
        #stickyClose {
            background: transparent;
            border: none;
            border-radius: 10px;
            color: #FFFFFF;
            font-size: 13px;
            font-weight: 600;
            padding: 0;
        }
        #stickyClose:hover {
            background: rgba(255,255,255,0.25);
        }
    )").arg(color));

    setObjectName("stickyCard");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 10);
    layout->setSpacing(6);

    QHBoxLayout *headRow = new QHBoxLayout();
    headRow->setSpacing(4);
    m_titleLabel = new QLabel(m_title, this);
    m_titleLabel->setObjectName("stickyTitle");
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    headRow->addWidget(m_titleLabel, 1);

    m_closeBtn = new QPushButton(tr("✕"), this);
    m_closeBtn->setObjectName("stickyClose");
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setToolTip(tr("关闭便签"));
    headRow->addWidget(m_closeBtn);
    layout->addLayout(headRow);

    m_contentEdit = new QTextEdit(this);
    m_contentEdit->setObjectName("stickyContent");
    m_contentEdit->setPlainText(content);
    m_contentEdit->setAcceptRichText(false);
    layout->addWidget(m_contentEdit, 1);

    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        saveContent();
        emit closeRequested(m_noteId);
        close();
    });
}

StickyNoteCard::~StickyNoteCard() = default;

void StickyNoteCard::setCardPosition(const QPoint &p)
{
    move(p);
}

void StickyNoteCard::showCard()
{
    show();
    raise();
}

void StickyNoteCard::saveContent()
{
    emit saved(m_noteId, m_contentEdit->toPlainText());
}

void StickyNoteCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QWidget::mousePressEvent(event);
}

void StickyNoteCard::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        emit moved(m_noteId, pos());
    }
    QWidget::mouseMoveEvent(event);
}

void StickyNoteCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        emit moved(m_noteId, pos());
    }
    QWidget::mouseReleaseEvent(event);
}

void StickyNoteCard::closeEvent(QCloseEvent *event)
{
    saveContent();
    emit closeRequested(m_noteId);
    QWidget::closeEvent(event);
}
