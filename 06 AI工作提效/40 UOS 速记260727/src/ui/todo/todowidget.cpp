// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "todowidget.h"
#include "application/shorthandapplication.h"
#include "core/todomanager.h"

#include <QScrollArea>
#include <QCheckBox>
#include <QHBoxLayout>
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

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget *container = new QWidget(scroll);
    m_mainLayout = new QVBoxLayout(container);
    m_mainLayout->setContentsMargins(16, 12, 16, 12);
    m_mainLayout->setSpacing(12);

    auto addSection = [this](const QString &title, const QString &color, QListWidget *&list) {
        DLabel *header = new DLabel(title, this);
        header->setStyleSheet(QString("font-size: 13px; font-weight: 600; color: %1; padding: 4px 0;").arg(color));
        m_mainLayout->addWidget(header);

        list = new QListWidget(this);
        list->setFrameShape(QFrame::NoFrame);
        list->setMaximumHeight(160);
        list->setSpacing(2);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
    addSection(tr("逾期待办"), "#E64545", m_overdueList);
    addSection(tr("本周待办"), "#FAAD14", m_weekList);
    addSection(tr("已完成"), "#52C41A", m_completedList);
    m_mainLayout->addStretch();

    scroll->setWidget(container);
    outerLayout->addWidget(scroll);
}

void TodoWidget::refresh()
{
    auto *app = ShorthandApplication::instance();
    auto *mgr = app->todoManager();

    QList<TodoData> todayTodos = mgr->getTodayTodos();
    QList<TodoData> overdueTodos = mgr->getOverdueTodos();
    QList<TodoData> completedTodos = mgr->getCompletedTodos();

    QList<TodoData> allTodos = mgr->getAllTodos();
    QList<TodoData> weekTodos;
    QDate today = QDate::currentDate();
    qint64 weekStart = QDateTime(today.addDays(-(int)today.dayOfWeek() + 1), QTime(0, 0)).toSecsSinceEpoch();
    qint64 weekEnd = QDateTime(today.addDays(7 - (int)today.dayOfWeek()), QTime(23, 59, 59)).toSecsSinceEpoch();

    for (const auto &todo : allTodos) {
        if (todo.dueDatetime >= weekStart && todo.dueDatetime <= weekEnd
            && !todo.isDueToday() && !todo.isOverdue()) {
            weekTodos.append(todo);
        }
    }

    populateSection(m_todayList, todayTodos, tr("今天没有待办"));
    populateSection(m_overdueList, overdueTodos, tr("没有逾期待办"));
    populateSection(m_weekList, weekTodos, tr("本周没有其他待办"));
    populateSection(m_completedList, completedTodos, tr("没有已完成事项"));
}

void TodoWidget::populateSection(QListWidget *list, const QList<TodoData> &todos, const QString &emptyHint)
{
    list->clear();
    if (todos.isEmpty()) {
        // 改进的空状态显示
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
        QString priorityStr = todo.priority == 3 ? "🔴" : todo.priority == 2 ? "🟡" : "";

        QWidget *card = new QWidget(this);
        QHBoxLayout *hLayout = new QHBoxLayout(card);
        hLayout->setContentsMargins(0, 0, 0, 0);
        hLayout->setSpacing(8);

        QCheckBox *checkBox = new QCheckBox(this);
        checkBox->setChecked(todo.isCompleted);
        int todoId = todo.id;

        QString displayText = priorityStr + " " + title;
        if (!dueStr.isEmpty()) displayText += QString("  [%1]").arg(dueStr);

        DLabel *textLabel = new DLabel(displayText, this);
        textLabel->setStyleSheet("font-size: 13px;");

        hLayout->addWidget(checkBox);
        hLayout->addWidget(textLabel, 1);

        QListWidgetItem *listItem = new QListWidgetItem(list);
        listItem->setData(Qt::UserRole, todoId);
        listItem->setSizeHint(card->sizeHint());

        connect(checkBox, &QCheckBox::toggled, this, [this, mgr = ShorthandApplication::instance()->todoManager(), todoId](bool checked) {
            mgr->toggleComplete(todoId, checked);
            emit todoStatusChanged();
            refresh();
        });

        list->addItem(listItem);
        list->setItemWidget(listItem, card);
    }
}

void TodoWidget::selectTodo(int todoId)
{
    emit todoSelected(todoId);
}
