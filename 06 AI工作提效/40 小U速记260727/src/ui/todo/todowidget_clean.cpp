// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "todowidget.h"
#include "application/shorthandapplication.h"
#include "core/todomanager.h"
#include "core/tagmanager.h"

#include <QScrollArea>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QUuid>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QSettings>
#include <QApplication>
#include <DLabel>
#include <DFontSizeManager>
#include <DGuiApplicationHelper>

TodoWidget::TodoWidget(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("TodoWidget { background: palette(base); }");
    loadSortPreference();
    initUI();
}

void TodoWidget::initUI()
{
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);

    // ─── 内容页面（输入 + 列表）────────────────────────────────
    m_contentWidget = new QWidget(this);
    QVBoxLayout *contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // 新建待办输入框（固定在顶部）
    QWidget *inputContainer = new QWidget(this);
    inputContainer->setStyleSheet("background: palette(base); border-bottom: 1px solid palette(midlight);");
    QHBoxLayout *inputRow = new QHBoxLayout(inputContainer);
    inputRow->setContentsMargins(16, 8, 16, 8);
    m_newTodoInput = new QLineEdit(this);
    m_newTodoInput->setPlaceholderText(tr("输入待办事项，按 Enter 创建"));
    m_newTodoInput->setStyleSheet(
        "QLineEdit { border: 1px solid palette(mid); border-radius: 6px;"
        " padding: 8px 12px; font-size: 13px; }");
    m_newTodoInput->setFixedHeight(36);
    inputRow->addWidget(m_newTodoInput);
    contentLayout->addWidget(inputContainer);

    // ─── 排序工具栏 ─────────────────────────────
    QWidget *sortBar = new QWidget(this);
    sortBar->setStyleSheet("background: transparent; padding: 4px 12px; border-bottom: 1px solid palette(midlight);");
    QHBoxLayout *sortLayout = new QHBoxLayout(sortBar);
    sortLayout->setContentsMargins(4, 4, 4, 4);
    sortLayout->setSpacing(6);

    DLabel *sortLabel = new DLabel(tr("排序:"), this);
    sortLabel->setStyleSheet("font-size: 11px; color: palette(placeholderText);");
    sortLayout->addWidget(sortLabel);

    m_sortFieldCombo = new QComboBox(this);
    m_sortFieldCombo->addItem(tr("截止日期"), TodoSortParam::DueDate);
    m_sortFieldCombo->addItem(tr("创建时间"), TodoSortParam::CreatedAt);
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

    sortLayout->addStretch();

    // ─── 多选模式切换按钮 ───────────────────────
    m_selectModeBtn = new QPushButton(tr("☐ 多选"), this);
    m_selectModeBtn->setCheckable(true);
    m_selectModeBtn->setFixedHeight(24);
    m_selectModeBtn->setStyleSheet(
        "QPushButton { border: 1px solid palette(mid); border-radius: 4px;"
        " padding: 2px 8px; font-size: 11px; background: palette(base); }"
        "QPushButton:hover { background: palette(light); }"
        "QPushButton:checked { background: palette(highlight); color: palette(highlightedText); }");
    sortLayout->addWidget(m_selectModeBtn);

    contentLayout->addWidget(sortBar);

    // 滚动区域（容纳四个分组列表）
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    QWidget *container = new QWidget(scroll);
    m_mainLayout = new QVBoxLayout(container);
    m_mainLayout->setContentsMargins(16, 12, 16, 12);
    m_mainLayout->setSpacing(12);

    auto addSection = [this](const QString &title, const QString &color, QListWidget *&list) {
        DLabel *header = new DLabel(title, this);
        header->setStyleSheet(
            QString("font-size: 13px; font-weight: 600; color: %1; padding: 4px 0;").arg(color));
        m_mainLayout->addWidget(header);

        list = new QListWidget(this);
        list->setFrameShape(QFrame::NoFrame);
        list->setMinimumHeight(48);
        list->setSpacing(2);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        list->setStyleSheet(R"(
            QListWidget { background: transparent; border: none; }
            QListWidget::item {
                border-radius: 6px; padding: 8px 12px; margin: 1px 0;
                background: palette(base); border: 1px solid palette(midlight);
            }
            QListWidget::item:hover { background: palette(midlight); border-color: palette(mid); }
        )");

        m_mainLayout->addWidget(list);
    };

    // 添加四个分区
    addSection(tr("⏰ 逾期"), "#E64545", m_overdueList);
    addSection(tr("📋 今日"), "#2178E5", m_todayList);
    addSection(tr("📅 本周"), "#52C41A", m_weekList);
    addSection(tr("✅ 已完成"), "#999999", m_completedList);

    scroll->setWidget(container);
    contentLayout->addWidget(scroll, 1);

    // ─── 批量操作工具栏 ─────────────────────────
    m_batchToolbar = new QWidget(this);
    m_batchToolbar->setStyleSheet("background: palette(midlight); border-radius: 8px; padding: 4px;");
    m_batchToolbar->hide();
    QHBoxLayout *batchLayout = new QHBoxLayout(m_batchToolbar);
    batchLayout->setContentsMargins(8, 4, 8, 4);
    batchLayout->setSpacing(6);

    m_selectionCountLabel = new DLabel(tr("已选择 0 项"), this);
    m_selectionCountLabel->setStyleSheet("font-size: 12px; color: palette(windowText);");
    batchLayout->addWidget(m_selectionCountLabel);

    batchLayout->addStretch();

    m_selectAllBtn = new QPushButton(tr("全选"), this);
    m_selectAllBtn->setFixedHeight(28);
    m_selectAllBtn->setStyleSheet(
        "QPushButton { border: 1px solid palette(mid); border-radius: 4px;"
        " padding: 2px 12px; font-size: 12px; background: palette(base); }"
        "QPushButton:hover { background: palette(light); }");
    batchLayout->addWidget(m_selectAllBtn);

    m_batchDeleteBtn = new QPushButton(tr("🗑 删除"), this);
    m_batchDeleteBtn->setFixedHeight(28);
    m_batchDeleteBtn->setStyleSheet(
        "QPushButton { border: 1px solid #E64545; border-radius: 4px;"
        " padding: 2px 12px; font-size: 12px; color: #E64545; background: palette(base); }"
        "QPushButton:hover { background: #FFF0F0; }");
    batchLayout->addWidget(m_batchDeleteBtn);

    contentLayout->addWidget(m_batchToolbar);

    m_stack->addWidget(m_contentWidget);

    // ─── 空状态页面 ────────────────────────────────────────────
    m_emptyWidget = new QWidget(this);
    QVBoxLayout *emptyLayout = new QVBoxLayout(m_emptyWidget);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(8);

    DLabel *emptyIcon = new DLabel(tr("✅"), this);
    emptyIcon->setStyleSheet("font-size: 48px;");
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyIcon);

    DLabel *emptyText = new DLabel(tr("所有待办已完成！"), this);
    emptyText->setStyleSheet("font-size: 14px; margin-top: 8px;");
    emptyText->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyText);

    // 为每个列表设置右键菜单
    setupListContextMenu(m_todayList);
    setupListContextMenu(m_overdueList);
    setupListContextMenu(m_weekList);
    setupListContextMenu(m_completedList);

    connect(m_newTodoInput, &QLineEdit::returnPressed, this, [this]() {
        QString text = m_newTodoInput->text().trimmed();
        if (text.isEmpty()) return;

        int priority = 0;
        // 解析 !! 优先级标记
        while (text.startsWith("!!")) {
            priority++;
            text = text.mid(2).trimmed();
        }
        if (priority > 3) priority = 3;

        TodoData todo;
        todo.title = text;
        todo.priority = priority;
        todo.creationDatetime = QDateTime::currentSecsSinceEpoch();
        todo.modificationDatetime = todo.creationDatetime;

        auto *app = ShorthandApplication::instance();
        app->todoManager()->createTodo(todo);
        m_newTodoInput->clear();
        refresh();
    });

    // 排序信号
    connect(m_sortFieldCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_sortParam.field = static_cast<TodoSortParam::Field>(m_sortFieldCombo->currentData().toInt());
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

    // ─── 多选模式切换 ───────────────────────────
    connect(m_selectModeBtn, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) {
            enterMultiSelectMode();
        } else {
            exitMultiSelectMode();
        }
    });

    // ─── 批量操作按钮 ───────────────────────────
    connect(m_selectAllBtn, &QPushButton::clicked, this, [this]() {
        // 收集所有列表中的复选框
        bool allSelected = true;
        auto checkAll = [&allSelected](QListWidget *list) {
            for (int i = 0; i < list->count(); ++i) {
                QListWidgetItem *item = list->item(i);
                QWidget *w = list->itemWidget(item);
                if (w) {
                    QCheckBox *cb = w->findChild<QCheckBox *>();
                    if (cb && !cb->isChecked()) {
                        allSelected = false;
                        return;
                    }
                }
            }
        };
        checkAll(m_overdueList);
        checkAll(m_todayList);
        checkAll(m_weekList);
        checkAll(m_completedList);

        bool check = !allSelected;
        auto setAll = [check](QListWidget *list) {
            for (int i = 0; i < list->count(); ++i) {
                QListWidgetItem *item = list->item(i);
                QWidget *w = list->itemWidget(item);
                if (w) {
                    QCheckBox *cb = w->findChild<QCheckBox *>();
                    if (cb) cb->setChecked(check);
                }
            }
        };
        setAll(m_overdueList);
        setAll(m_todayList);
        setAll(m_weekList);
        setAll(m_completedList);
        updateSelectionState();
    });

    connect(m_batchDeleteBtn, &QPushButton::clicked, this, &TodoWidget::onBatchDelete);

    m_stack->addWidget(m_emptyWidget);
    outerLayout->addWidget(m_stack);
}

// ─── 多选模式 ────────────────────────────────────────────────

void TodoWidget::enterMultiSelectMode()
{
    m_multiSelectMode = true;
    refresh();
    m_batchToolbar->show();
    updateSelectionState();
}

void TodoWidget::exitMultiSelectMode()
{
    m_multiSelectMode = false;
    m_batchToolbar->hide();
    refresh();
}

void TodoWidget::updateSelectionState()
{
    QList<int> selected = getSelectedTodoIds();
    int count = selected.size();
    m_selectionCountLabel->setText(tr("已选择 %1 项").arg(count));
    m_batchDeleteBtn->setEnabled(count > 0);
}

QList<int> TodoWidget::getSelectedTodoIds() const
{
    QList<int> ids;
    auto collectIds = [&ids](QListWidget *list) {
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem *item = list->item(i);
            QWidget *w = list->itemWidget(item);
            if (w) {
                QCheckBox *cb = w->findChild<QCheckBox *>();
                if (cb && cb->isChecked()) {
                    int todoId = item->data(Qt::UserRole).toInt();
                    if (todoId > 0) ids.append(todoId);
                }
            }
        }
    };
    collectIds(m_overdueList);
    collectIds(m_todayList);
    collectIds(m_weekList);
    collectIds(m_completedList);
    return ids;
}

void TodoWidget::onBatchDelete()
{
    QList<int> selectedIds = getSelectedTodoIds();
    if (selectedIds.isEmpty()) return;

    QMessageBox::StandardButton btn = QMessageBox::question(
        this, tr("删除待办"),
        tr("确定要将选中的 %1 条待办移至回收站吗？").arg(selectedIds.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (btn != QMessageBox::Yes) return;

    auto *app = ShorthandApplication::instance();
    app->todoManager()->batchDeleteTodos(selectedIds);

    exitMultiSelectMode();
    refresh();
    emit todoStatusChanged();
}

// ─── 原有功能 ────────────────────────────────────────────────

void TodoWidget::refresh()
{
    auto *app = ShorthandApplication::instance();
    TodoManager *mgr = app->todoManager();

    // 获取各分区的待办数据
    QList<TodoData> overdueTodos = mgr->getOverdueTodos(m_sortParam);
    QList<TodoData> todayTodos = mgr->getTodayTodos(m_sortParam);
    QList<TodoData> weekTodos = mgr->getWeekTodos(m_sortParam);
    QList<TodoData> completedTodos = mgr->getCompletedTodos();

    // 过滤掉今日和逾期已包含的
    QList<TodoData> weekOnly;
    QSet<int> todayIds, overdueIds;
    for (const auto &t : todayTodos) todayIds.insert(t.id);
    for (const auto &t : overdueTodos) overdueIds.insert(t.id);
    for (const auto &t : weekTodos) {
        if (!todayIds.contains(t.id) && !overdueIds.contains(t.id))
            weekOnly.append(t);
    }

    populateSection(m_overdueList, overdueTodos, tr("没有逾期的待办"), m_overdueCount);
    populateSection(m_todayList, todayTodos, tr("今天没有待办"), m_todayCount);
    populateSection(m_weekList, weekOnly, tr("本周没有其他待办"), m_weekCount);
    populateSection(m_completedList, completedTodos, tr("还没有已完成的待办"), m_completedCount);

    updateOverallEmptyState();
}

void TodoWidget::selectTodo(int todoId)
{
    emit todoSelected(todoId);
}

void TodoWidget::populateSection(QListWidget *list, const QList<TodoData> &todos,
                                  const QString &emptyHint, int &outCount, bool isPreset)
{
    list->clear();
    outCount = todos.size();

    if (todos.isEmpty()) {
        // 改进的空行提示
        QWidget *emptyWidget = new QWidget(this);
        QHBoxLayout *emptyLayout = new QHBoxLayout(emptyWidget);
        emptyLayout->setContentsMargins(12, 8, 12, 8);

        DLabel *emptyLabel = new DLabel(emptyHint, this);
        emptyLabel->setStyleSheet("color: palette(placeholderText); font-size: 12px;");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyLabel);

        QListWidgetItem *item = new QListWidgetItem(list);
        item->setFlags(Qt::NoItemFlags);
        item->setSizeHint(QSize(0, 36));
        list->addItem(item);
        list->setItemWidget(item, emptyWidget);
        return;
    }

    for (const auto &todo : todos) {
        QString title = todo.title.isEmpty() ? tr("无标题") : todo.title;
        QString dueStr = todo.dueDatetime > 0
            ? QDateTime::fromSecsSinceEpoch(todo.dueDatetime).toString("MM-dd")
            : "";
        QString priorityStr = todo.priority >= 3 ? "🔴"
                            : todo.priority == 2 ? "🟡"
                            : "";
        QString tagStr = todo.tag.isEmpty() ? "" : QString(" [%1]").arg(todo.tag);

        QWidget *card = new QWidget(this);
        QHBoxLayout *hLayout = new QHBoxLayout(card);
        hLayout->setContentsMargins(0, 0, 0, 0);
        hLayout->setSpacing(4);

        // 多选复选框（仅多选模式下显示，替代原有的勾选框）
        QCheckBox *multiCb = nullptr;
        if (m_multiSelectMode) {
            multiCb = new QCheckBox(this);
            multiCb->setFixedSize(24, 24);
            multiCb->setStyleSheet("QCheckBox::indicator { width: 16px; height: 16px; }");
            hLayout->addWidget(multiCb);
        }

        QCheckBox *checkBox = new QCheckBox(this);
        checkBox->setChecked(todo.isCompleted);
        if (isPreset) {
            // 预置示例 — 只做视觉引导，不触发数据库写入
            checkBox->setEnabled(false);
            checkBox->setStyleSheet("QCheckBox::indicator { opacity: 0.5; }");
        }
        int todoId = todo.id;

        QString displayText = priorityStr + " " + title + tagStr;
        if (!dueStr.isEmpty()) displayText += QString("  [%1]").arg(dueStr);

        // 逾期待办标红
        QString textStyle = "font-size: 13px;";
        if (todo.isOverdue() && !todo.isCompleted) {
            textStyle += " color: #E64545;";
        }
        if (isPreset) {
            textStyle += " font-style: italic; opacity: 0.7;";
        }

        DLabel *textLabel = new DLabel(displayText, this);
        textLabel->setStyleSheet(textStyle);

        hLayout->addWidget(checkBox);
        hLayout->addWidget(textLabel, 1);

        QListWidgetItem *listItem = new QListWidgetItem(list);
        listItem->setData(Qt::UserRole, todoId);
        listItem->setSizeHint(card->sizeHint());

        if (!isPreset && !m_multiSelectMode) {
            connect(checkBox, &QCheckBox::toggled, this,
                    [this, mgr = ShorthandApplication::instance()->todoManager(),
                     todoId](bool checked) {
                        mgr->toggleComplete(todoId, checked);
                        emit todoStatusChanged();
                        refresh();
                    });
        }

        list->addItem(listItem);
        list->setItemWidget(listItem, card);
    }
}

void TodoWidget::setupListContextMenu(QListWidget *list)
{
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QListWidget::customContextMenuRequested, this, &TodoWidget::onContextMenu);
}

QList<TodoData> TodoWidget::presetExamples() const
{
    QList<TodoData> examples;
    qint64 now = QDateTime::currentSecsSinceEpoch();

    auto makeExample = [now](int id, const QString &title, int priority, qint64 dueOffsetSecs) {
        TodoData t;
        t.id = id;
        t.title = title;
        t.priority = priority;
        t.creationDatetime = now;
        t.dueDatetime = (dueOffsetSecs != 0) ? now + dueOffsetSecs : 0;
        return t;
    };

    // 今日（id -1, -2）
    examples.append(makeExample(-1, tr("🖊️ 试试输入待办事项"), 0, 3600));
    examples.append(makeExample(-2, tr("📌 高优先级用 !! 开头"), 3, 7200));
    // 逾期（id -3）
    examples.append(makeExample(-3, tr("⏰ 逾期的待办会显示在这里"), 2, -86400));
    // 本周（id -4）
    examples.append(makeExample(-4, tr("📋 点击复选框标记完成"), 0, 86400 * 2));
    // 已完成（id -5）
    {
        TodoData t;
        t.id = -5;
        t.title = tr("🎉 欢迎使用 UOS速记");
        t.isCompleted = true;
        examples.append(t);
    }

    return examples;
}

/// 根据 section 名称返回对应的预设示例列表

{
    // 找出右键点击的列表和项
    QListWidget *list = qobject_cast<QListWidget *>(sender());
    if (!list) return;

    QListWidgetItem *item = list->itemAt(pos);
    if (!item) return;

    int todoId = item->data(Qt::UserRole).toInt();
    if (todoId <= 0) return; // 预设示例不弹出菜单

    QMenu *menu = createTodoContextMenu(todoId);
    if (menu) {
        menu->exec(list->viewport()->mapToGlobal(pos));
        delete menu;
    }
}

QMenu *TodoWidget::createTodoContextMenu(int todoId)
{
    auto *app = ShorthandApplication::instance();
    TodoManager *mgr = app->todoManager();

    TodoData todo = mgr->getTodo(todoId);
    if (todo.id <= 0) return nullptr;

    QMenu *menu = new QMenu(this);
    menu->setStyleSheet(R"(
        QMenu {
            background: palette(base);
            border: 1px solid palette(mid);
            border-radius: 8px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px 6px 12px;
            border-radius: 4px;
            font-size: 13px;
        }
        QMenu::item:selected {
            background: palette(highlight);
            color: palette(highlightedText);
        }
        QMenu::separator {
            height: 1px;
            margin: 4px 8px;
            background: palette(midlight);
        }
    )");

    // 完成/取消完成
    QAction *toggleAction = menu->addAction(
        todo.isCompleted ? tr("取消完成") : tr("标记完成"));
    connect(toggleAction, &QAction::triggered, this, [this, todoId, completed = todo.isCompleted]() {
        auto *mgr = ShorthandApplication::instance()->todoManager();
        mgr->toggleComplete(todoId, !completed);
        emit todoStatusChanged();
        refresh();
    });

    menu->addSeparator();

    // 设置标签子菜单
    buildTagSubMenu(menu, todoId);

    // 设置优先级子菜单
    buildPrioritySubMenu(menu, todoId);

    menu->addSeparator();

    // 删除（带确认提示）
    QAction *deleteAction = menu->addAction(tr("删除"));
    connect(deleteAction, &QAction::triggered, this, [this, todoId]() {
        QMessageBox::StandardButton btn = QMessageBox::question(
            this, tr("删除待办"),
            tr("确定要将该待办移至回收站吗？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes) return;

        ShorthandApplication::instance()->todoManager()->deleteTodo(todoId);
        emit todoStatusChanged();
        refresh();
    });

    return menu;
}

void TodoWidget::buildTagSubMenu(QMenu *parentMenu, int todoId)
{
    QMenu *tagMenu = parentMenu->addMenu(tr("设置标签"));
    auto *app = ShorthandApplication::instance();
    TodoData todo = app->todoManager()->getTodo(todoId);

    // "无标签" 选项
    QAction *actNoTag = tagMenu->addAction(tr("无标签"));
    actNoTag->setCheckable(true);
    actNoTag->setChecked(todo.tag.isEmpty());
    connect(actNoTag, &QAction::triggered, this, [this, todoId]() {
        bool ok = ShorthandApplication::instance()->todoManager()->setTag(todoId, "");
        if (!ok) {
            QMessageBox::warning(const_cast<TodoWidget*>(this), tr("错误"), tr("清除标签失败"));
        }
        refresh();
    });
    tagMenu->addSeparator();

    QList<TagData> tags = app->tagManager()->getAllTags();
    for (const TagData &tag : tags) {
        QAction *act = tagMenu->addAction(tag.name);
        act->setCheckable(true);
        act->setChecked(todo.tag == tag.name);
        connect(act, &QAction::triggered, this, [this, todoId, name = tag.name]() {
            bool ok = ShorthandApplication::instance()->todoManager()->setTag(todoId, name);
            if (!ok) {
                QMessageBox::warning(const_cast<TodoWidget*>(this), tr("错误"), tr("设置标签失败"));
            }
            refresh();
        });
    }
}

void TodoWidget::buildPrioritySubMenu(QMenu *parentMenu, int todoId)
{
    QMenu *priorityMenu = parentMenu->addMenu(tr("设置优先级"));
    TodoData todo = ShorthandApplication::instance()->todoManager()->getTodo(todoId);

    struct PrioItem { QString label; int value; };
    QList<PrioItem> items = {
        {tr("无优先级"), 0},
        {tr("🟢 低"), 1},
        {tr("🟡 中"), 2},
        {tr("🔴 高"), 3}
    };

    for (const auto &pi : items) {
        QAction *act = priorityMenu->addAction(pi.label);
        act->setCheckable(true);
        act->setChecked(todo.priority == pi.value);
        connect(act, &QAction::triggered, this, [this, todoId, prio = pi.value]() {
            bool ok = ShorthandApplication::instance()->todoManager()->setPriority(todoId, prio);
            if (!ok) {
                QMessageBox::warning(const_cast<TodoWidget*>(this), tr("错误"), tr("设置优先级失败"));
            }
            refresh();
        });
    }
}
QList<TodoData> TodoWidget::presetExamplesForSection(const QString &section) const
{
    QList<TodoData> all = presetExamples();
    QList<TodoData> result;

    // 根据 id 负值区分归属分区
    for (const auto &t : all) {
        bool match = false;
        if (section == "today") {
            match = (t.id >= -2);
        } else if (section == "overdue") {
            match = (t.id == -3);
        } else if (section == "week") {
            match = (t.id == -4);
        } else if (section == "completed") {
            match = (t.id == -5);
        }
        if (match) result.append(t);
    }
    return result;
}

void TodoWidget::updateOverallEmptyState()
{
    // 当前由 refresh() 末尾统一处理，本函数保留为接口
    bool hasContent = m_todayCount > 0 || m_overdueCount > 0
        || m_weekCount > 0 || m_completedCount > 0;
    m_stack->setCurrentWidget(hasContent ? m_contentWidget : m_emptyWidget);
}

// ─── 排序偏好 ────────────────────────────────────────────────

void TodoWidget::loadSortPreference()
{
    QSettings settings("deepin", "uos-shorthand");
    m_sortParam.field = static_cast<TodoSortParam::Field>(
        settings.value("todos_sort_field", static_cast<int>(TodoSortParam::DueDate)).toInt());
    m_sortParam.ascending = settings.value("todos_sort_ascending", true).toBool();
}

void TodoWidget::saveSortPreference()
{
    QSettings settings("deepin", "uos-shorthand");
    settings.setValue("todos_sort_field", static_cast<int>(m_sortParam.field));
    settings.setValue("todos_sort_ascending", m_sortParam.ascending);
}
