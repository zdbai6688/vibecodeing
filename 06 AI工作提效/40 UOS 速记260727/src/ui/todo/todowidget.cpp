// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "todowidget.h"
#include "application/shorthandapplication.h"
#include "core/todomanager.h"
#include "core/tagmanager.h"

#include <QHBoxLayout>
#include <QRegularExpression>
#include <QDateTime>
#include <QMessageBox>
#include <DLabel>
#include <DFontSizeManager>

// 待办列表：每条显示标题+创建时间+标签+复选框，未完成/已完成分区
TodoWidget::TodoWidget(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("TodoWidget { background: palette(base); }");
    initUI();
}

QWidget *TodoWidget::createSectionHeader(const QString &title, const QString &color)
{
    QWidget *header = new QWidget(this);
    QVBoxLayout *hl = new QVBoxLayout(header);
    hl->setContentsMargins(0, 8, 0, 2);
    DLabel *label = new DLabel(title, this);
    label->setStyleSheet(QString("font-size: 12px; font-weight: 600; color: %1;").arg(color));
    hl->addWidget(label);
    return header;
}

void TodoWidget::initUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(4);

    // 顶部行：新建待办输入框 + 多选按钮
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    m_newTodoInput = new QLineEdit(this);
    m_newTodoInput->setPlaceholderText(tr("新建待办，输入内容按 Enter 创建"));
    m_newTodoInput->setFixedHeight(36);
    m_newTodoInput->setStyleSheet(
        "QLineEdit { border: 1px solid #EAECEF; border-radius: 6px;"
        " padding: 8px 12px; font-size: 13px; background: palette(base); }"
        "QLineEdit:focus { border-color: #2178E5; }");
    topRow->addWidget(m_newTodoInput, 1);

    // 多选模式切换按钮
    m_selectModeBtn = new QPushButton(tr("☐ 多选"), this);
    m_selectModeBtn->setCheckable(true);
    m_selectModeBtn->setFixedHeight(36);
    m_selectModeBtn->setFixedWidth(60);
    m_selectModeBtn->setStyleSheet(
        "QPushButton { border: 1px solid palette(mid); border-radius: 6px;"
        " padding: 4px 8px; font-size: 12px; background: palette(base); }"
        "QPushButton:hover { background: palette(light); }"
        "QPushButton:checked { background: palette(highlight); color: white; border-color: palette(highlight); }");
    topRow->addWidget(m_selectModeBtn);

    layout->addLayout(topRow);

    connect(m_newTodoInput, &QLineEdit::returnPressed, this, [this]() {
        QString text = m_newTodoInput->text().trimmed();
        if (text.isEmpty()) return;

        int priority = 0;
        QStringList tags;

        QRegularExpression prioRe(QStringLiteral(R"(!([高中低]))"));
        QRegularExpressionMatch prioMatch = prioRe.match(text);
        if (prioMatch.hasMatch()) {
            QString p = prioMatch.captured(1);
            if (p == QString::fromUtf8("\xe9\xab\x98")) priority = 3;
            else if (p == QString::fromUtf8("\xe4\xb8\xad")) priority = 2;
            else if (p == QString::fromUtf8("\xe4\xbd\x8e")) priority = 1;
            text = text.mid(0, prioMatch.capturedStart()) + text.mid(prioMatch.capturedEnd());
            text = text.trimmed();
        }

        QRegularExpression tagRe(QStringLiteral(R"(#(\S+))"));
        QRegularExpressionMatchIterator tagMatches = tagRe.globalMatch(text);
        while (tagMatches.hasNext()) {
            QRegularExpressionMatch m = tagMatches.next();
            QString tagName = m.captured(1).trimmed();
            if (!tagName.isEmpty()) tags.append(tagName);
        }
        text = text.remove(tagRe).trimmed();
        if (text.isEmpty()) return;

        qint64 parsedDue = 0;
        QDate today = QDate::currentDate();
        if (text.contains(QString::fromUtf8("\xe6\x98\x8e\xe5\xa4\xa9"))) {
            parsedDue = QDateTime(today.addDays(1), QTime(23, 59, 59)).toSecsSinceEpoch();
            text.replace(QString::fromUtf8("\xe6\x98\x8e\xe5\xa4\xa9"), "");
        } else if (text.contains(QString::fromUtf8("\xe5\x90\x8e\xe5\xa4\xa9"))) {
            parsedDue = QDateTime(today.addDays(2), QTime(23, 59, 59)).toSecsSinceEpoch();
            text.replace(QString::fromUtf8("\xe5\x90\x8e\xe5\xa4\xa9"), "");
        } else {
            QLocale chLocale(QLocale::Chinese);
            for (int d = 1; d <= 7; d++) {
                QString dayName = chLocale.dayName(d, QLocale::LongFormat);
                if (text.contains(dayName)) {
                    int curDay = today.dayOfWeek();
                    int daysUntil = d - curDay;
                    if (daysUntil <= 0) daysUntil += 7;
                    parsedDue = QDateTime(today.addDays(daysUntil), QTime(23, 59, 59)).toSecsSinceEpoch();
                    text.replace(dayName, "");
                    break;
                }
            }
        }
        text = text.trimmed();
        if (text.isEmpty()) return;

        TodoData todo;
        todo.title = text;
        todo.priority = priority;
        todo.tags = tags;
        todo.dueDatetime = parsedDue;
        todo.creationDatetime = QDateTime::currentSecsSinceEpoch();
        todo.modificationDatetime = todo.creationDatetime;

        auto *app = ShorthandApplication::instance();
        for (const QString &tagName : tags) {
            if (!app->tagManager()->allTagNames().contains(tagName, Qt::CaseInsensitive)) {
                app->tagManager()->createTag(tagName, "#1890FF");
            }
        }
        app->todoManager()->createTodo(todo);
        m_newTodoInput->clear();
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

    // 未完成列表（含分区标题）
    layout->addWidget(createSectionHeader(tr("进行中"), "#2178E5"));
    m_pendingList = new QListWidget(this);
    m_pendingList->setFrameShape(QFrame::NoFrame);
    m_pendingList->setSpacing(4);
    m_pendingList->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; }
        QListWidget::item { border-radius: 8px; background: palette(light); margin: 2px 0; }
        QListWidget::item:hover { background: palette(midlight); }
        QListWidget::item:selected { background: palette(highlight); }
    )");
    layout->addWidget(m_pendingList);

    // 已完成列表（含分区标题）
    layout->addWidget(createSectionHeader(tr("已完成"), "#999999"));
    m_completedList = new QListWidget(this);
    m_completedList->setFrameShape(QFrame::NoFrame);
    m_completedList->setSpacing(4);
    m_completedList->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; }
        QListWidget::item { border-radius: 8px; background: palette(light); margin: 2px 0; }
        QListWidget::item:hover { background: palette(midlight); }
    )");
    layout->addWidget(m_completedList, 1);

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

    m_batchDeleteBtn = new QPushButton(tr("🗑 删除"), this);
    m_batchDeleteBtn->setFixedHeight(28);
    m_batchDeleteBtn->setStyleSheet(
        "QPushButton { border: 1px solid palette(mid); border-radius: 4px;"
        " padding: 2px 10px; font-size: 11px; background: palette(base); }"
        "QPushButton:hover { background: #E64545; color: white; }");
    batchLayout->addWidget(m_batchDeleteBtn);

    layout->addWidget(m_batchToolbar);

    // ─── 信号连接 ───────────────────────────────
    connect(m_pendingList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        showTodoContextMenu(m_pendingList, pos);
    });
    connect(m_pendingList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        int todoId = item->data(Qt::UserRole).toInt();
        if (m_multiSelectMode) {
            QWidget *w = m_pendingList->itemWidget(item);
            if (w) {
                QCheckBox *cb = w->findChild<QCheckBox *>("selectCheck");
                if (cb) {
                    cb->setChecked(!cb->isChecked());
                    updateSelectionState();
                }
            }
        } else if (todoId > 0) {
            emit todoSelected(todoId);
        }
    });

    connect(m_completedList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        int todoId = item->data(Qt::UserRole).toInt();
        if (m_multiSelectMode) {
            QWidget *w = m_completedList->itemWidget(item);
            if (w) {
                QCheckBox *cb = w->findChild<QCheckBox *>("selectCheck");
                if (cb) {
                    cb->setChecked(!cb->isChecked());
                    updateSelectionState();
                }
            }
        }
    });

    // 全选按钮
    connect(m_selectAllBtn, &QPushButton::clicked, this, [this]() {
        bool allSelected = true;
        auto checkAll = [&allSelected](QListWidget *list) {
            for (int i = 0; i < list->count(); ++i) {
                QWidget *w = list->itemWidget(list->item(i));
                if (w) {
                    QCheckBox *cb = w->findChild<QCheckBox *>("selectCheck");
                    if (cb && !cb->isChecked()) {
                        allSelected = false;
                        return;
                    }
                }
            }
        };
        checkAll(m_pendingList);
        checkAll(m_completedList);

        bool check = !allSelected;
        auto setAll = [check](QListWidget *list) {
            for (int i = 0; i < list->count(); ++i) {
                QWidget *w = list->itemWidget(list->item(i));
                if (w) {
                    QCheckBox *cb = w->findChild<QCheckBox *>("selectCheck");
                    if (cb) cb->setChecked(check);
                }
            }
        };
        setAll(m_pendingList);
        setAll(m_completedList);
        updateSelectionState();
    });

    // 批量删除
    connect(m_batchDeleteBtn, &QPushButton::clicked, this, &TodoWidget::onBatchDelete);
}

void TodoWidget::enterMultiSelectMode()
{
    m_multiSelectMode = true;
    m_batchToolbar->show();
    refresh();
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
    auto collect = [&ids](QListWidget *list) {
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem *item = list->item(i);
            QWidget *w = list->itemWidget(item);
            if (w) {
                QCheckBox *cb = w->findChild<QCheckBox *>("selectCheck");
                if (cb && cb->isChecked()) {
                    int todoId = item->data(Qt::UserRole).toInt();
                    if (todoId > 0) ids.append(todoId);
                }
            }
        }
    };
    collect(m_pendingList);
    collect(m_completedList);
    return ids;
}

void TodoWidget::onBatchDelete()
{
    QList<int> ids = getSelectedTodoIds();
    if (ids.isEmpty()) return;

    auto *app = ShorthandApplication::instance();
    if (!app || !app->todoManager()) return;

    auto reply = QMessageBox::question(this, tr("删除待办"),
                                       tr("确定要删除选中的 %1 条待办吗？").arg(ids.size()),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    app->todoManager()->batchDeleteTodos(ids);
    m_selectModeBtn->setChecked(false);
    refresh();
}

void TodoWidget::showTodoContextMenu(QListWidget *list, const QPoint &pos)
{
    QListWidgetItem *item = list->itemAt(pos);
    if (!item || m_multiSelectMode) return;
    int todoId = item->data(Qt::UserRole).toInt();
    if (todoId <= 0) return;

    auto *app = ShorthandApplication::instance();
    if (!app || !app->todoManager()) return;

    TodoData todo = app->todoManager()->getTodo(todoId);
    if (todo.id <= 0) return;

    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu { background: palette(window); border: 1px solid palette(mid); border-radius: 6px; padding: 4px; }
        QMenu::item { padding: 6px 24px; border-radius: 4px; font-size: 13px; }
        QMenu::item:selected { background: palette(highlight); color: palette(highlightedText); }
        QMenu::separator { height: 1px; background: palette(midlight); margin: 4px 8px; }
    )");

    QAction *toggleAction = menu.addAction(todo.isCompleted ? tr("标记未完成") : tr("标记完成"));
    connect(toggleAction, &QAction::triggered, this, [this, todoId, todo]() {
        ShorthandApplication::instance()->todoManager()->toggleComplete(todoId, !todo.isCompleted);
        emit todoStatusChanged();
        refresh();
    });

    menu.addSeparator();

    QAction *deleteAction = menu.addAction(tr("删除"));
    connect(deleteAction, &QAction::triggered, this, [this, todoId]() {
        ShorthandApplication::instance()->todoManager()->deleteTodo(todoId);
        refresh();
    });

    menu.exec(list->viewport()->mapToGlobal(pos));
}

void TodoWidget::setTodoDueDate(int todoId)
{
    DDialog dialog(this);
    dialog.setTitle(tr("设置截止日期"));

    QDateEdit *dateEdit = new QDateEdit(QDate::currentDate(), &dialog);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    dialog.addContent(dateEdit);

    dialog.addButton(tr("取消"));
    dialog.addButton(tr("确定"), true, DDialog::ButtonRecommend);
    if (dialog.exec() == 1) {
        auto *app = ShorthandApplication::instance();
        TodoData todo = app->todoManager()->getTodo(todoId);
        if (todo.id > 0) {
            todo.dueDatetime = QDateTime(dateEdit->date(), QTime(23, 59, 59)).toSecsSinceEpoch();
            app->todoManager()->updateTodo(todo);
            refresh();
        }
    }
}

QWidget *TodoWidget::createTodoRow(const TodoData &todo)
{
    QWidget *card = new QWidget(this);
    QHBoxLayout *hLayout = new QHBoxLayout(card);
    hLayout->setContentsMargins(12, 6, 12, 6);
    hLayout->setSpacing(8);

    // 多选模式下的选择复选框（默认隐藏）
    QCheckBox *selectBox = new QCheckBox(card);
    selectBox->setObjectName("selectCheck");
    selectBox->setFixedSize(20, 20);
    selectBox->setVisible(m_multiSelectMode);
    hLayout->addWidget(selectBox);

    // 完成复选框
    QCheckBox *checkBox = new QCheckBox(card);
    checkBox->setChecked(todo.isCompleted);
    checkBox->setFixedSize(20, 20);
    hLayout->addWidget(checkBox);

    // 标题 + 创建时间（竖排）
    QWidget *textWidget = new QWidget(card);
    QVBoxLayout *tl = new QVBoxLayout(textWidget);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(2);

    QString title = todo.title.isEmpty() ? tr("无标题") : todo.title;
    DLabel *titleLabel = new DLabel(title, card);
    titleLabel->setStyleSheet(todo.isCompleted
        ? "font-size: 13px; color: #BBBBBB; text-decoration: line-through;"
        : "font-size: 13px; color: #222222;");
    tl->addWidget(titleLabel);

    // 创建时间 + 截止日期
    QString timeStr = tr("创建于 ") + todo.createdAt().toString("MM-dd HH:mm");
    if (todo.dueDatetime > 0) {
        timeStr += tr("  截止 ") + QDateTime::fromSecsSinceEpoch(todo.dueDatetime).toString("MM-dd");
    }
    DLabel *timeLabel = new DLabel(timeStr, card);
    timeLabel->setStyleSheet("font-size: 10px; color: #999999;");
    tl->addWidget(timeLabel);

    hLayout->addWidget(textWidget, 1);

    // 优先级
    if (todo.priority >= 3) {
        DLabel *p = new DLabel(tr("高"), card);
        p->setStyleSheet("background: #E64545; color: white; font-size: 10px; border-radius: 4px; padding: 1px 6px;");
        hLayout->addWidget(p);
    } else if (todo.priority == 2) {
        DLabel *p = new DLabel(tr("中"), card);
        p->setStyleSheet("background: #FAAD14; color: white; font-size: 10px; border-radius: 4px; padding: 1px 6px;");
        hLayout->addWidget(p);
    }

    // 标签徽章
    QStringList tagNames = todo.tags;
    if (tagNames.isEmpty() && !todo.tag.isEmpty()) tagNames << todo.tag;
    if (!tagNames.isEmpty()) {
        auto *app = ShorthandApplication::instance();
        QList<TagData> allTags = app->tagManager()->getAllTags();
        for (const QString &tn : tagNames) {
            QString color = "#2178E5";
            for (const auto &t : allTags) {
                if (t.name == tn) { color = t.color; break; }
            }
            QLabel *badge = new QLabel(tn, card);
            badge->setFixedHeight(18);
            badge->setStyleSheet(QString(
                "QLabel { background: %1; color: white; font-size: 10px; font-weight: 600;"
                " border-radius: 9px; padding: 0 6px; }").arg(color));
            hLayout->addWidget(badge);
        }
    }

    int todoId = todo.id;
    connect(checkBox, &QCheckBox::toggled, this, [this, todoId](bool checked) {
        ShorthandApplication::instance()->todoManager()->toggleComplete(todoId, checked);
        emit todoStatusChanged();
        refresh();
    });

    // 多选模式切换时显示/隐藏选择框
    connect(m_selectModeBtn, &QPushButton::toggled, selectBox, &QCheckBox::setVisible);

    // 选择框变化时更新选中计数
    connect(selectBox, &QCheckBox::toggled, this, &TodoWidget::updateSelectionState);

    return card;
}

void TodoWidget::populateList(const QList<TodoData> &todos)
{
    m_pendingList->clear();
    m_completedList->clear();

    QList<TodoData> pending, completed;
    for (const auto &t : todos) {
        if (t.isCompleted) completed.append(t);
        else pending.append(t);
    }

    auto fill = [this](QListWidget *list, const QList<TodoData> &items) {
        for (const auto &t : items) {
            QWidget *row = createTodoRow(t);
            QListWidgetItem *item = new QListWidgetItem(list);
            item->setData(Qt::UserRole, t.id);
            item->setSizeHint(QSize(0, 52));
            list->addItem(item);
            list->setItemWidget(item, row);
        }
    };

    if (pending.isEmpty()) {
        QListWidgetItem *hint = new QListWidgetItem(tr("没有待办事项，点击「+ 新建待办」，或从会议纪要中提取"));
        hint->setFlags(Qt::NoItemFlags);
        hint->setForeground(QColor("#BBBBBB"));
        hint->setTextAlignment(Qt::AlignCenter);
        m_pendingList->addItem(hint);
    } else {
        fill(m_pendingList, pending);
    }

    if (!completed.isEmpty()) {
        fill(m_completedList, completed);
    }
}

void TodoWidget::refresh()
{
    auto *app = ShorthandApplication::instance();
    TodoManager *mgr = app->todoManager();

    QList<TodoData> all = mgr->getAllTodos();

    if (!m_filterTags.isEmpty()) {
        all.erase(std::remove_if(all.begin(), all.end(), [this](const TodoData &t) {
            for (const QString &ft : m_filterTags) {
                if (!t.tags.contains(ft) && t.tag != ft) return true;
            }
            return false;
        }), all.end());
    }

    populateList(all);
}

void TodoWidget::selectTodo(int todoId)
{
    emit todoSelected(todoId);
}

void TodoWidget::setFilterTags(const QStringList &tags)
{
    m_filterTags = tags;
    refresh();
}

void TodoWidget::focusNewTodoInput()
{
    m_newTodoInput->setFocus();
    m_newTodoInput->selectAll();
}
