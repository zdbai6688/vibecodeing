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
#include <DSpinner>
#include <QPushButton>
#include <QSettings>
#include <QDebug>
#include <QCheckBox>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include "services/exportservice.h"

NoteListWidget::NoteListWidget(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("NoteListWidget { background: palette(base); }");
    loadSortPreference();
    initUI();
}

void NoteListWidget::initUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(0);

    m_stack = new QStackedWidget(this);

    m_spinner = new DSpinner(this);
    m_spinner->setFixedSize(32, 32);
    QWidget *spinnerWrapper = new QWidget(this);
    QVBoxLayout *spinnerLayout = new QVBoxLayout(spinnerWrapper);
    spinnerLayout->setAlignment(Qt::AlignCenter);
    spinnerLayout->addWidget(m_spinner);
    m_stack->addWidget(spinnerWrapper);

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

    m_stack->addWidget(m_list);
    // ─── 排序/多选工具栏 ─────────────────────────
    QWidget *m_sortBar = new QWidget(this);
    m_sortBar->setStyleSheet("background: transparent; padding: 4px 0;");
    QHBoxLayout *sortLayout = new QHBoxLayout(m_sortBar);
    sortLayout->setContentsMargins(0, 0, 0, 4);
    sortLayout->setSpacing(6);

    DLabel *sortLabel = new DLabel(tr("排序:"), this);
    sortLabel->setStyleSheet("font-size: 11px; color: palette(placeholderText);");
    sortLayout->addWidget(sortLabel);

    m_sortFieldCombo = new QComboBox(this);
    m_sortFieldCombo->addItem(tr("修改时间"), NoteSortParam::ModifiedAt);
    m_sortFieldCombo->addItem(tr("创建时间"), NoteSortParam::CreatedAt);
    m_sortFieldCombo->setCurrentIndex(static_cast<int>(m_sortParam.field));
    m_sortFieldCombo->setStyleSheet(
        "QComboBox { border: 1px solid palette(mid); border-radius: 4px;"
        " padding: 2px 8px; font-size: 11px; background: palette(base);"
        " min-width: 80px; }");
    m_sortFieldCombo->setFixedHeight(24);
    sortLayout->addWidget(m_sortFieldCombo);

    m_sortOrderBtn = new DToolButton(this);
    m_sortOrderBtn->setText(m_sortParam.ascending ? "↑" : "↓");
    m_sortOrderBtn->setToolTip(m_sortParam.ascending ? tr("升序") : tr("降序"));
    m_sortOrderBtn->setFixedSize(24, 24);
    m_sortOrderBtn->setStyleSheet(
        "DToolButton { border: 1px solid palette(mid); border-radius: 4px;"
        " font-size: 12px; background: palette(base); }"
        "DToolButton:hover { background: palette(light); }");
    sortLayout->addWidget(m_sortOrderBtn);

    // ─── 多选模式切换按钮 ───────────────────────
    m_selectModeBtn = new QPushButton(tr("☐ 多选"), this);
    m_selectModeBtn->setCheckable(true);
    m_selectModeBtn->setFixedHeight(24);
    m_selectModeBtn->setStyleSheet(
        "QPushButton { border: 1px solid palette(mid); border-radius: 4px;"
        " padding: 2px 8px; font-size: 11px; background: palette(base); }"
        "QPushButton:hover { background: palette(light); }"
        "QPushButton:checked { background: palette(highlight); color: white; border-color: palette(highlight); }");
    sortLayout->addWidget(m_selectModeBtn);

    sortLayout->addStretch();

    layout->addWidget(m_sortBar);

    // ─── 批量操作工具栏（底部，多选模式时显示） ──
    m_batchToolbar = new QWidget(this);
    m_batchToolbar->setStyleSheet("background: palette(midlight); border-radius: 8px; padding: 4px;");
    m_batchToolbar->hide();
    QHBoxLayout *batchLayout = new QHBoxLayout(m_batchToolbar);
    batchLayout->setContentsMargins(8, 4, 8, 4);
    batchLayout->setSpacing(6);

    m_selectionCountLabel = new DLabel(tr("已选择 0 项"), this);
    m_selectionCountLabel->setStyleSheet("font-size: 11px; color: palette(windowText);");
    batchLayout->addWidget(m_selectionCountLabel);

    batchLayout->addStretch();

    m_selectAllBtn = new QPushButton(tr("全选"), this);
    m_selectAllBtn->setFixedHeight(28);
    m_selectAllBtn->setStyleSheet(
        "QPushButton { border: 1px solid palette(mid); border-radius: 4px;"
        " padding: 2px 10px; font-size: 11px; background: palette(base); }"
        "QPushButton:hover { background: palette(highlight); color: white; }");
    batchLayout->addWidget(m_selectAllBtn);

    m_batchExportBtn = new QPushButton(tr("📤 导出"), this);
    m_batchExportBtn->setFixedHeight(28);
    m_batchExportBtn->setStyleSheet(
        "QPushButton { border: 1px solid palette(mid); border-radius: 4px;"
        " padding: 2px 10px; font-size: 11px; background: palette(base); }"
        "QPushButton:hover { background: palette(highlight); color: white; }");
    batchLayout->addWidget(m_batchExportBtn);

    m_batchDeleteBtn = new QPushButton(tr("🗑 删除"), this);
    m_batchDeleteBtn->setFixedHeight(28);
    m_batchDeleteBtn->setStyleSheet(
        "QPushButton { border: 1px solid palette(mid); border-radius: 4px;"
        " padding: 2px 10px; font-size: 11px; background: palette(base); }"
        "QPushButton:hover { background: #E64545; color: white; }");
    batchLayout->addWidget(m_batchDeleteBtn);

    layout->addWidget(m_stack, 1);
    layout->addWidget(m_batchToolbar);

    // ─── 信号连接 ───────────────────────────────
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        int noteId = item->data(Qt::UserRole).toInt();
        if (m_multiSelectMode) {
            // 多选模式下点击切换复选框
            QWidget *w = m_list->itemWidget(item);
            if (w) {
                QCheckBox *cb = w->findChild<QCheckBox *>();
                if (cb) {
                    cb->setChecked(!cb->isChecked());
                    updateSelectionState();
                }
            }
        } else if (noteId > 0) {
            emit noteSelected(noteId);
        }
    });

    // ─── 排序控件信号 ───────────────────────────
    connect(m_sortFieldCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_sortParam.field = static_cast<NoteSortParam::Field>(m_sortFieldCombo->currentData().toInt());
        saveSortPreference();
        refresh();
    });
    connect(m_sortOrderBtn, &DToolButton::clicked, this, [this]() {
        m_sortParam.ascending = !m_sortParam.ascending;
        m_sortOrderBtn->setText(m_sortParam.ascending ? "↑" : "↓");
        m_sortOrderBtn->setToolTip(m_sortParam.ascending ? tr("升序") : tr("降序"));
        saveSortPreference();
        refresh();
    });

    // ─── 多选模式信号 ───────────────────────────
    connect(m_selectModeBtn, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) {
            enterMultiSelectMode();
        } else {
            exitMultiSelectMode();
        }
    });

    // ─── 批量操作信号 ───────────────────────────
    connect(m_selectAllBtn, &QPushButton::clicked, this, [this]() {
        bool allSelected = true;
        for (int i = 0; i < m_list->count(); ++i) {
            QListWidgetItem *item = m_list->item(i);
            QWidget *w = m_list->itemWidget(item);
            if (w) {
                QCheckBox *cb = w->findChild<QCheckBox *>();
                if (cb && !cb->isChecked()) {
                    allSelected = false;
                    break;
                }
            }
        }
        bool check = !allSelected;
        for (int i = 0; i < m_list->count(); ++i) {
            QListWidgetItem *item = m_list->item(i);
            QWidget *w = m_list->itemWidget(item);
            if (w) {
                QCheckBox *cb = w->findChild<QCheckBox *>();
                if (cb) cb->setChecked(check);
            }
        }
        updateSelectionState();
    });

    connect(m_batchDeleteBtn, &QPushButton::clicked, this, &NoteListWidget::onBatchDelete);
    connect(m_batchExportBtn, &QPushButton::clicked, this, &NoteListWidget::onBatchExport);
}

void NoteListWidget::enterMultiSelectMode()
{
    m_multiSelectMode = true;
    m_batchToolbar->show();
    refresh();
    updateSelectionState();
}

void NoteListWidget::exitMultiSelectMode()
{
    m_multiSelectMode = false;
    m_batchToolbar->hide();
    refresh();
}

void NoteListWidget::updateSelectionState()
{
    QList<int> selected = getSelectedNoteIds();
    int count = selected.size();
    m_selectionCountLabel->setText(tr("已选择 %1 项").arg(count));
    m_batchDeleteBtn->setEnabled(count > 0);
    m_batchExportBtn->setEnabled(count > 0);
}

QList<int> NoteListWidget::getSelectedNoteIds() const
{
    QList<int> ids;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        QWidget *w = m_list->itemWidget(item);
        if (w) {
            QCheckBox *cb = w->findChild<QCheckBox *>();
            if (cb && cb->isChecked()) {
                int noteId = item->data(Qt::UserRole).toInt();
                if (noteId > 0) ids.append(noteId);
            }
        }
    }
    return ids;
}

void NoteListWidget::onBatchDelete()
{
    QList<int> ids = getSelectedNoteIds();
    if (ids.isEmpty()) return;

    ShorthandApplication *app = ShorthandApplication::instance();
    if (!app || !app->noteManager()) return;

    QString title;
    if (m_mode == Trash) {
        // 回收站模式：永久删除
        auto reply = QMessageBox::question(this, tr("永久删除"),
                                           tr("确定要永久删除选中的 %1 条笔记吗？\n此操作不可恢复。").arg(ids.size()),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        app->noteManager()->batchPermanentDelete(ids);
    } else {
        // 普通模式：移至回收站
        auto reply = QMessageBox::question(this, tr("删除笔记"),
                                           tr("确定要将选中的 %1 条笔记移至回收站吗？").arg(ids.size()),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        app->noteManager()->batchDeleteNotes(ids);
    }

    m_selectModeBtn->setChecked(false); // 退出多选模式
    refresh();
}

void NoteListWidget::onBatchExport()
{
    QList<int> ids = getSelectedNoteIds();
    if (ids.isEmpty()) return;

    ShorthandApplication *app = ShorthandApplication::instance();
    if (!app || !app->noteManager()) return;

    // 弹出格式选择菜单
    QMenu menu(this);
    QAction *mdAction = menu.addAction(tr("Markdown (.md)"));
    QAction *txtAction = menu.addAction(tr("纯文本 (.txt)"));
    QAction *zipAction = menu.addAction(tr("ZIP 打包"));

    QAction *chosen = menu.exec(QCursor::pos());
    if (!chosen) return;

    QString format;
    if (chosen == mdAction) format = "md";
    else if (chosen == txtAction) format = "txt";
    else if (chosen == zipAction) format = "zip";
    else return;

    // 收集选中的笔记数据
    QList<NoteData> notes;
    for (int id : ids) {
        NoteData note = app->noteManager()->getNote(id);
        if (note.id > 0) notes.append(note);
    }

    if (notes.isEmpty()) return;

    // 选择保存目录
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择导出目录"));
    if (dir.isEmpty()) return;

    ExportService *exportService = app->exportService();
    if (!exportService) {
        qWarning() << "ExportService not available";
        return;
    }

    bool ok = false;
    if (format == "zip") {
        ok = exportService->exportNotesToZip(notes, dir);
    } else {
        // 逐个导出
        ok = true;
        for (const NoteData &note : notes) {
            QString safeName = ExportService::sanitizeFileName(note.title.isEmpty() ? "untitled" : note.title);
            QString filePath = dir + "/" + safeName + "." + format;
            if (format == "md") {
                ok = exportService->exportNoteToMarkdown(note, filePath);
            } else {
                ok = exportService->exportNoteToTxt(note, filePath);
            }
            if (!ok) break;
        }
    }

    if (ok) {
        QMessageBox::information(this, tr("导出成功"),
                                 tr("已成功导出 %1 条笔记到:\n%2").arg(notes.size()).arg(dir));
    } else {
        QMessageBox::warning(this, tr("导出失败"), tr("导出过程中出现错误，请重试。"));
    }
}

void NoteListWidget::loadSortPreference()
{
    QSettings settings;
    int field = settings.value("notes/sort_field", static_cast<int>(NoteSortParam::ModifiedAt)).toInt();
    bool ascending = settings.value("notes/sort_order", false).toBool();
    m_sortParam.field = static_cast<NoteSortParam::Field>(field);
    m_sortParam.ascending = ascending;
}

void NoteListWidget::saveSortPreference()
{
    QSettings settings;
    settings.setValue("notes/sort_field", static_cast<int>(m_sortParam.field));
    settings.setValue("notes/sort_order", m_sortParam.ascending);
}

void NoteListWidget::setMode(Mode mode)
{
    m_mode = mode;
    // 回收站模式下隐藏多选按钮
    m_selectModeBtn->setVisible(mode != Trash);
    if (mode == Trash && m_multiSelectMode) {
        m_selectModeBtn->setChecked(false);
        exitMultiSelectMode();
    }
}

void NoteListWidget::setFilterTag(const QString &tag)
{
    m_filterTag = tag;
    m_mode = TagFilter;
    m_selectModeBtn->setVisible(true);
}

void NoteListWidget::setSearchKeyword(const QString &keyword)
{
    m_searchKeyword = keyword;
    m_mode = Search;
    m_selectModeBtn->setVisible(true);
}

void NoteListWidget::refresh()
{
    ShorthandApplication *app = ShorthandApplication::instance();
    if (!app || !app->noteManager()) {
        showLoading(false);
        return;
    }

    showLoading(true);

    QList<NoteData> notes;
    switch (m_mode) {
    case Trash:
        notes = app->noteManager()->getDeletedNotes();
        break;
    case TagFilter:
        notes = app->noteManager()->getNotesByTag(m_filterTag, m_sortParam);
        break;
    case Search:
        notes = app->noteManager()->searchNotes(m_searchKeyword, m_sortParam);
        break;
    default:
        notes = app->noteManager()->getAllNotes(m_sortParam);
    }

    populateList(notes);
    showLoading(false);
}

void NoteListWidget::showLoading(bool loading)
{
    if (loading) {
        m_spinner->start();
        m_stack->setCurrentWidget(m_spinner->parentWidget());
    } else {
        m_spinner->stop();
        m_stack->setCurrentWidget(m_list);
    }
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
        QHBoxLayout *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(4, 8, 12, 8);
        cardLayout->setSpacing(6);

        // 多选模式下的复选框
        QCheckBox *checkBox = nullptr;
        if (m_multiSelectMode) {
            checkBox = new QCheckBox(this);
            checkBox->setStyleSheet(
                "QCheckBox { spacing: 4px; }"
                "QCheckBox::indicator { width: 18px; height: 18px; }");
            cardLayout->addWidget(checkBox, 0, Qt::AlignCenter);
        }

        QVBoxLayout *textLayout = new QVBoxLayout();
        textLayout->setContentsMargins(checkBox ? 0 : 8, 0, 0, 0);
        textLayout->setSpacing(3);

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
        textLayout->addLayout(topRow);

        if (!preview.isEmpty()) {
            DLabel *previewLabel = new DLabel(preview, this);
            previewLabel->setStyleSheet("font-size: 11px; color: palette(windowText);");
            previewLabel->setWordWrap(true);
            previewLabel->setFixedHeight(20);
            textLayout->addWidget(previewLabel);
        }

        if (!tagStr.isEmpty()) {
            DLabel *tagLabel = new DLabel(tagStr, this);
            tagLabel->setStyleSheet("font-size: 10px; color: palette(highlight);");
            textLayout->addWidget(tagLabel);
        }

        cardLayout->addLayout(textLayout, 1);

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
