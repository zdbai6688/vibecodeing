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
            QListWidget::item:hover { background: palette(light); }
        )");
        setupListContextMenu(list);
        m_mainLayout->addWidget(list);
    };

    addSection(tr("今日待办"), "palette(highlight)", m_todayList);
    addSection(tr("逾期待办"), "palette(highlight)", m_overdueList);
    addSection(tr("本周待办"), "palette(highlight)", m_weekList);
    addSection(tr("已完成"), "palette(highlight)", m_completedList);
    m_mainLayout->addStretch();

    scroll->setWidget(container);
    contentLayout->addWidget(scroll, 1);
    m_stack->addWidget(m_contentWidget);

    // ─── 空状态页面（无任何预设/待办时的兜底）─────────────────
    m_emptyWidget = new QWidget(this);
    QVBoxLayout *emptyLayout = new QVBoxLayout(m_emptyWidget);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(8);
    DLabel *emptyIcon = new DLabel(tr("✅"), m_emptyWidget);
    emptyIcon->setStyleSheet("font-size: 48px;");
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyIcon);
    DLabel *emptyTitle = new DLabel(tr("还没有待办事项"), m_emptyWidget);
    emptyTitle->setStyleSheet("font-size: 14px;");
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);
    DLabel *emptyHint = new DLabel(
        tr("在上方输入框中输入文字，按 Enter 创建你的第一个待办"), m_emptyWidget);
    emptyHint->setStyleSheet("color: palette(placeholderText); font-size: 12px;");
    emptyHint->setAlignment(Qt::AlignCenter);
    emptyHint->setWordWrap(true);
    emptyLayout->addWidget(emptyHint);
    m_stack->addWidget(m_emptyWidget);

    outerLayout->addWidget(m_stack);

    // ─── 连接信号 ────────────────────────────────
    connect(m_newTodoInput, &QLineEdit::returnPressed, this, [this]() {
        QString text = m_newTodoInput->text().trimmed();
        if (text.isEmpty()) return;
        m_newTodoInput->clear();

        TodoData todo;
        todo.title = text;
        todo.creationDatetime = QDateTime::currentSecsSinceEpoch();
        todo.modificationDatetime = todo.creationDatetime;

        // 解析优先级标记：!! 高，! 中
        if (text.startsWith("!!")) {
            todo.priority = 3;
            todo.title = text.mid(2).trimmed();
        } else if (text.startsWith("!")) {
            todo.priority = 2;
            todo.title = text.mid(1).trimmed();
        }

        auto *mgr = ShorthandApplication::instance()->todoManager();
        int id = mgr->createTodo(todo);
        if (id > 0) {
            refresh();
        }
    });

    // ─── 排序控件信号 ───────────────────────────
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
}

void TodoWidget::loadSortPreference()
{
    QSettings settings;
    int field = settings.value("todos/sort_field", static_cast<int>(TodoSortParam::DueDate)).toInt();
    bool ascending = settings.value("todos/sort_order", true).toBool();
    m_sortParam.field = static_cast<TodoSortParam::Field>(field);
    m_sortParam.ascending = ascending;
}

void TodoWidget::saveSortPreference()
{
    QSettings settings;
    settings.setValue("todos/sort_field", static_cast<int>(m_sortParam.field));
    settings.setValue("todos/sort_order", m_sortParam.ascending);
}

void TodoWidget::setupListContextMenu(QListWidget *list)
{
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QListWidget::customContextMenuRequested, this, &TodoWidget::onContextMenu);
}

void TodoWidget::onContextMenu(const QPoint &pos)
{
    QListWidget *list = qobject_cast<QListWidget *>(sender());
    if (!list) return;

    QListWidgetItem *item = list->itemAt(pos);
    if (!item) return;

    int todoId = item->data(Qt::UserRole).toInt();
    if (todoId <= 0) return; // 跳过预置示例（负ID）

    // 禁用右键菜单的待办 → 获取最新数据
    auto *mgr = ShorthandApplication::instance()->todoManager();
    TodoData todo = mgr->getTodo(todoId);
    if (todo.id <= 0) return; // 待办不存在

    QMenu *menu = createTodoContextMenu(todoId);
    if (menu) {
        menu->exec(list->viewport()->mapToGlobal(pos));
        delete menu;
    }
}

QMenu *TodoWidget::createTodoContextMenu(int todoId)
{
    auto *mgr = ShorthandApplication::instance()->todoManager();
    TodoData todo = mgr->getTodo(todoId);
    if (todo.id <= 0) return nullptr;

    QMenu *menu = new QMenu(this);
    menu->setStyleSheet(R"(
        QMenu {
            background: palette(window);
            border: 1px solid palette(mid);
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px;
            border-radius: 4px;
        }
        QMenu::item:selected { background: palette(highlight); color: palette(highlightedText); }
        QMenu::separator { height: 1px; background: palette(midlight); margin: 4px 8px; }
    )");

    // ─── 1. 完成/取消完成 ─────────────────────────
    QAction *toggleAction = menu->addAction(
        todo.isCompleted ? tr("取消完成") : tr("标记完成"));
    connect(toggleAction, &QAction::triggered, this, [this, mgr, todoId, todo]() {
        mgr->toggleComplete(todoId, !todo.isCompleted);
        refresh();
    });

    menu->addSeparator();

    // ─── 2. 设置标签 ──────────────────────────────
    QMenu *tagMenu = menu->addMenu(tr("设置标签"));
    buildTagSubMenu(tagMenu, todoId);

    // ─── 3. 优先级 ────────────────────────────────
    QMenu *priorityMenu = menu->addMenu(tr("优先级"));
    buildPrioritySubMenu(priorityMenu, todoId);

    menu->addSeparator();

    // ─── 4. 删除 ──────────────────────────────────
    QAction *deleteAction = menu->addAction(tr("删除待办"));
    deleteAction->setIcon(QIcon()); // 可使用系统图标
    connect(deleteAction, &QAction::triggered, this, [this, mgr, todoId]() {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("删除待办"));
        msgBox.setText(tr("确定要将待办移至回收站吗？"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        msgBox.button(QMessageBox::Yes)->setText(tr("确定"));
        msgBox.button(QMessageBox::No)->setText(tr("取消"));
        if (msgBox.exec() == QMessageBox::Yes) {
            mgr->deleteTodo(todoId);
            refresh();
        }
    });

    return menu;
}

void TodoWidget::buildTagSubMenu(QMenu *parentMenu, int todoId)
{
    auto *tagMgr = ShorthandApplication::instance()->tagManager();
    auto *todoMgr = ShorthandApplication::instance()->todoManager();
    TodoData todo = todoMgr->getTodo(todoId);

    QStringList tagNames = tagMgr->allTagNames();
    for (const QString &tagName : tagNames) {
        QAction *tagAction = parentMenu->addAction(tagName);
        if (todo.tag == tagName) {
            tagAction->setCheckable(true);
            tagAction->setChecked(true);
        }
        connect(tagAction, &QAction::triggered, this, [this, todoMgr, todoId, tagName]() {
            todoMgr->setTag(todoId, tagName);
            refresh();
        });
    }

    // 分隔线 + "无标签" 选项
    if (!tagNames.isEmpty()) {
        parentMenu->addSeparator();
    }
    QAction *noTagAction = parentMenu->addAction(tr("无标签"));
    if (todo.tag.isEmpty()) {
        noTagAction->setCheckable(true);
        noTagAction->setChecked(true);
    }
    connect(noTagAction, &QAction::triggered, this, [this, todoMgr, todoId]() {
        todoMgr->setTag(todoId, QString());
        refresh();
    });
}

void TodoWidget::buildPrioritySubMenu(QMenu *parentMenu, int todoId)
{
    auto *mgr = ShorthandApplication::instance()->todoManager();
    TodoData todo = mgr->getTodo(todoId);

    struct PrioEntry {
        QString label;
        int value;
    };
    const PrioEntry entries[] = {
        { tr("高"), 3 },
        { tr("中"), 2 },
        { tr("低"), 1 },
        { tr("无"), 0 },
    };

    for (const auto &entry : entries) {
        QAction *action = parentMenu->addAction(entry.label);
        if (todo.priority == entry.value) {
            action->setCheckable(true);
            action->setChecked(true);
        }
        connect(action, &QAction::triggered, this, [this, mgr, todoId, val = entry.value]() {
            mgr->setPriority(todoId, val);
            refresh();
        });
    }
}

void TodoWidget::refresh()
{
    auto *mgr = ShorthandApplication::instance()->todoManager();

    // 使用排序参数获取数据
    QList<TodoData> todayTodos = mgr->getTodayTodos(m_sortParam);
    QList<TodoData> overdueTodos = mgr->getOverdueTodos(m_sortParam);
    QList<TodoData> weekTodos = mgr->getPendingTodos(m_sortParam);
    QList<TodoData> completedTodos = mgr->getCompletedTodos();

    // 从 weekTodos 中去掉 today 和 overdue 的项（避免重复）
    {
        QSet<int> dedup;
        for (const auto &t : todayTodos) dedup.insert(t.id);
        for (const auto &t : overdueTodos) dedup.insert(t.id);
        weekTodos.erase(std::remove_if(weekTodos.begin(), weekTodos.end(),
                        [&](const TodoData &t) { return dedup.contains(t.id); }),
                        weekTodos.end());
    }

    // 判断是否为首次空状态 → 显示预设示例
    bool hasAnyData = mgr->pendingCount() > 0 || mgr->completedCount() > 0;
    bool usePresets = !hasAnyData;

    populateSection(m_todayList,
                    usePresets ? presetExamplesForSection("today") : todayTodos,
                    tr("没有今日待办"), m_todayCount, usePresets);
    populateSection(m_overdueList,
                    usePresets ? presetExamplesForSection("overdue") : overdueTodos,
                    tr("没有逾期待办"), m_overdueCount, usePresets);
    populateSection(m_weekList,
                    usePresets ? presetExamplesForSection("week") : weekTodos,
                    tr("本周没有其他待办"), m_weekCount, usePresets);
    populateSection(m_completedList,
                    usePresets ? presetExamplesForSection("completed") : completedTodos,
                    tr("没有已完成事项"), m_completedCount, usePresets);

    // 空状态切换：有预设时展示内容页（显示预设引导），无预设且无待办时展示空状态页
    bool hasRealContent = m_todayCount > 0 || m_overdueCount > 0
        || m_weekCount > 0 || m_completedCount > 0;
    m_stack->setCurrentWidget(hasRealContent ? m_contentWidget : m_emptyWidget);
}

/// 返回预置示例待办列表（负 ID 标记，仅用于空状态引导）
QList<TodoData> TodoWidget::presetExamples() const
{
    QList<TodoData> examples;

    auto makeExample = [&](int id, const QString &title, int priority, qint64 dueOffsetSecs) {
        TodoData t;
        t.id = id;
        t.title = title;
        t.priority = priority;
        if (dueOffsetSecs >= 0)
            t.dueDatetime = QDateTime::currentSecsSinceEpoch() + dueOffsetSecs;
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
        hLayout->setSpacing(8);

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

        if (!isPreset) {
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

void TodoWidget::selectTodo(int todoId)
{
    emit todoSelected(todoId);
}
