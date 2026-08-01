// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "todowidget.h"
#include "application/shorthandapplication.h"
#include "core/todomanager.h"

#include <QScrollArea>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QUuid>
#include <DLabel>
#include <DFontSizeManager>
#include <DGuiApplicationHelper>

TodoWidget::TodoWidget(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("TodoWidget { background: palette(base); }");
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

    // ─── 连接信号 ──────────────────────────────────
    connect(m_newTodoInput, &QLineEdit::returnPressed, this, [this]() {
        QString text = m_newTodoInput->text().trimmed();
        if (text.isEmpty()) return;
        TodoData todo;
        todo.title = text;
        todo.creationDatetime = QDateTime::currentSecsSinceEpoch();
        todo.modificationDatetime = todo.creationDatetime;
        // 以 ! 开头 → 高优先级，!! 开头 → 紧急
        if (text.startsWith("!!")) {
            todo.priority = 3;
        } else if (text.startsWith('!')) {
            todo.priority = 2;
        } else {
            todo.priority = 1;
        }
        auto *app = ShorthandApplication::instance();
        if (app->todoManager()->createTodo(todo) > 0) {
            m_newTodoInput->clear();
            refresh();
        }
    });
}

/// 构造预置示例待办列表（仅当用户尚无任何待办时展示）
QList<TodoData> TodoWidget::presetExamples() const
{
    QList<TodoData> examples;
    qint64 now = QDateTime::currentSecsSinceEpoch();
    QDate today = QDate::currentDate();
    qint64 todayStart = QDateTime(today, QTime(0, 0)).toSecsSinceEpoch();

    // 今日示例
    TodoData d1;
    d1.id = -1;
    d1.title = tr("了解待办分组视图 — 今日/逾期/本周/已完成");
    d1.priority = 2;
    d1.creationDatetime = now;
    d1.dueDatetime = todayStart + 3600 * 14;
    examples.append(d1);

    TodoData d2;
    d2.id = -2;
    d2.title = tr("按 Enter 或点击上方输入框快速新建待办");
    d2.priority = 1;
    d2.creationDatetime = now;
    d2.dueDatetime = todayStart + 3600 * 18;
    examples.append(d2);

    // 逾期示例
    TodoData d3;
    d3.id = -3;
    d3.title = tr("逾期的待办会标红显示，提醒你尽快处理");
    d3.priority = 3;
    d3.creationDatetime = now - 86400 * 2;
    d3.dueDatetime = todayStart - 86400;
    examples.append(d3);

    // 本周示例
    TodoData d4;
    d4.id = -4;
    d4.title = tr("使用 ! 优先级标记：!!紧急  !高  中(默认)");
    d4.priority = 1;
    d4.creationDatetime = now;
    d4.dueDatetime = todayStart + 86400 * 3;
    examples.append(d4);

    // 已完成示例
    TodoData d5;
    d5.id = -5;
    d5.title = tr("勾选待办前面的方框，完成它会移至「已完成」");
    d5.priority = 1;
    d5.isCompleted = true;
    d5.creationDatetime = now - 86400;
    d5.completedDatetime = now - 3600;
    examples.append(d5);

    return examples;
}

void TodoWidget::refresh()
{
    auto *app = ShorthandApplication::instance();
    auto *mgr = app->todoManager();

    QList<TodoData> todayTodos = mgr->getTodayTodos();
    QList<TodoData> overdueTodos = mgr->getOverdueTodos();
    QList<TodoData> completedTodos = mgr->getCompletedTodos();

    QList<TodoData> allTodos = mgr->getAllTodos();
    int realTodoCount = allTodos.size();

    QList<TodoData> weekTodos;
    QDate today = QDate::currentDate();
    qint64 weekStart =
        QDateTime(today.addDays(-(int)today.dayOfWeek() + 1), QTime(0, 0)).toSecsSinceEpoch();
    qint64 weekEnd =
        QDateTime(today.addDays(7 - (int)today.dayOfWeek()), QTime(23, 59, 59)).toSecsSinceEpoch();

    for (const auto &todo : allTodos) {
        if (todo.dueDatetime >= weekStart && todo.dueDatetime <= weekEnd
            && !todo.isDueToday() && !todo.isOverdue()) {
            weekTodos.append(todo);
        }
    }

    // 如果没有任何真实待办，用预置示例填充各分组（引导用户理解功能）
    bool usePresets = (realTodoCount == 0);

    populateSection(m_todayList,
                    usePresets ? presetExamplesForSection("today") : todayTodos,
                    tr("今天没有待办"), m_todayCount, usePresets);
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

        QString displayText = priorityStr + " " + title;
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
