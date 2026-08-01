// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "notelistwidget.h"
#include "application/shorthandapplication.h"
#include "core/notemanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <DLabel>
#include <DFontSizeManager>
#include <DGuiApplicationHelper>
#include <DGuiApplicationHelper>
#include <QDebug>

NoteListWidget::NoteListWidget(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("NoteListWidget { background: palette(base); }");
    initUI();
}

void NoteListWidget::initUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(0);

    m_list = new QListWidget(this);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSpacing(4);
    m_list->setWordWrap(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 使用 palette 关键词让 QSS 自动适应主题
    m_list->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; padding: 4px 0; }
        QListWidget::item {
            border-radius: 6px; padding: 0; margin: 2px 0;
            background: palette(light); border: 1px solid transparent;
        }
        QListWidget::item:hover { background: palette(midlight); border-color: palette(mid); }
        QListWidget::item:selected { background: palette(highlight); border-color: palette(highlight); }
    )");

    layout->addWidget(m_list, 1);

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        int noteId = item->data(Qt::UserRole).toInt();
        if (noteId > 0) emit noteSelected(noteId);
    });
}

void NoteListWidget::setMode(Mode mode) { m_mode = mode; }
void NoteListWidget::setFilterTag(const QString &tag) { m_filterTag = tag; }
void NoteListWidget::setSearchKeyword(const QString &keyword) { m_searchKeyword = keyword; }

void NoteListWidget::refresh()
{
    auto *app = ShorthandApplication::instance();
    QList<NoteData> notes;

    switch (m_mode) {
    case AllNotes:
        notes = app->noteManager()->getAllNotes();
        break;
    case TagFilter:
        notes = app->noteManager()->getNotesByTag(m_filterTag);
        break;
    case Trash:
        notes = app->noteManager()->getDeletedNotes();
        break;
    case Search:
        notes = m_searchKeyword.isEmpty() ? app->noteManager()->getAllNotes()
                                          : app->noteManager()->searchNotes(m_searchKeyword);
        break;
    default:
        notes = app->noteManager()->getAllNotes();
    }

    populateList(notes);
}

void NoteListWidget::selectNote(int noteId)
{
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        if (item->data(Qt::UserRole).toInt() == noteId) {
            m_list->setCurrentItem(item);
            emit noteSelected(noteId);
            return;
        }
    }
}

void NoteListWidget::populateList(const QList<NoteData> &notes)
{
    m_list->clear();

    if (notes.isEmpty()) {
        QWidget *emptyWidget = new QWidget(this);
        QVBoxLayout *emptyLayout = new QVBoxLayout(emptyWidget);
        emptyLayout->setAlignment(Qt::AlignCenter);
        emptyLayout->setSpacing(8);

        DLabel *emptyIcon = new DLabel(getEmptyIcon(m_mode), this);
        emptyIcon->setStyleSheet("font-size: 48px;");
        emptyIcon->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyIcon);

        DLabel *emptyText = new DLabel(getEmptyTitle(m_mode), this);
        emptyText->setStyleSheet("font-size: 14px; margin-top: 8px;");
        emptyText->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyText);

        DLabel *emptyHint = new DLabel(getEmptyHint(m_mode), this);
        emptyHint->setStyleSheet("color: palette(placeholderText); font-size: 12px; margin-top: 4px; line-height: 1.6;");
        emptyHint->setAlignment(Qt::AlignCenter);
        emptyHint->setWordWrap(true);
        emptyLayout->addWidget(emptyHint);

        QListWidgetItem *item = new QListWidgetItem();
        item->setSizeHint(QSize(m_list->width() - 24, 200));
        m_list->addItem(item);
        m_list->setItemWidget(item, emptyWidget);
        return;
    }

    for (const auto &note : notes) {
        QString title = note.title.isEmpty() ? tr("无标题") : note.title;
        QString preview = note.previewText(100);
        QString timeStr = note.createdAt().toString("MM-dd HH:mm");
        QString tagStr = note.tag.isEmpty() ? "" : QString("  #%1").arg(note.tag);

        QWidget *card = new QWidget(this);
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 8, 12, 8);
        cardLayout->setSpacing(3);

        QHBoxLayout *topRow = new QHBoxLayout();
        topRow->setSpacing(6);
        DLabel *titleLabel = new DLabel(title, this);
        titleLabel->setStyleSheet("font-size: 13px; font-weight: 600;");
        titleLabel->setFixedHeight(20);
        topRow->addWidget(titleLabel, 1);
        topRow->addStretch();
        DLabel *timeLabel = new DLabel(timeStr, this);
        timeLabel->setStyleSheet("font-size: 10px; color: palette(placeholderText);");
        timeLabel->setFixedHeight(20);
        topRow->addWidget(timeLabel);
        cardLayout->addLayout(topRow);

        if (!preview.isEmpty()) {
            DLabel *previewLabel = new DLabel(preview, this);
            previewLabel->setStyleSheet("font-size: 11px; color: palette(windowText);");
            previewLabel->setWordWrap(true);
            previewLabel->setFixedHeight(20);
            cardLayout->addWidget(previewLabel);
        }

        if (!tagStr.isEmpty()) {
            DLabel *tagLabel = new DLabel(tagStr, this);
            tagLabel->setStyleSheet("font-size: 10px; color: palette(highlight);");
            cardLayout->addWidget(tagLabel);
        }

        QListWidgetItem *item = new QListWidgetItem();
        item->setData(Qt::UserRole, note.id);
        card->setFixedHeight(72);
        item->setSizeHint(QSize(0, 72));
        m_list->addItem(item);
        m_list->setItemWidget(item, card);
    }
}

QString NoteListWidget::getEmptyIcon(Mode mode) const
{
    switch (mode) {
    case AllNotes: return tr("📝");
    case TagFilter: return tr("🏷️");
    case Trash: return tr("🗑️");
    case Search: return tr("🔍");
    default: return tr("📄");
    }
}

QString NoteListWidget::getEmptyTitle(Mode mode) const
{
    switch (mode) {
    case AllNotes: return tr("还没有笔记");
    case TagFilter: return tr("该标签下暂无笔记");
    case Trash: return tr("回收站为空");
    case Search: return tr("未找到匹配的笔记");
    default: return tr("暂无内容");
    }
}

QString NoteListWidget::getEmptyHint(Mode mode) const
{
    switch (mode) {
    case AllNotes: return tr("点击左下角「+ 新建」按钮开始记录\n首行为标题，下方内容自由书写");
    case Trash: return tr("删除的笔记会出现在这里");
    case Search: return tr("尝试更换搜索关键词");
    default: return QString();
    }
}
