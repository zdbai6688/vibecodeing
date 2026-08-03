// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "todowidget.h"
#include "application/shorthandapplication.h"
#include "core/todomanager.h"
#include "core/tagmanager.h"

#include <QHBoxLayout>
#include <QRegularExpression>
#include <QDateTime>
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

    // 顶部：新建待办输入框
    m_newTodoInput = new QLineEdit(this);
    m_newTodoInput->setPlaceholderText(tr("新建待办，输入内容按 Enter 创建"));
    m_newTodoInput->setFixedHeight(36);
    m_newTodoInput->setStyleSheet(
        "QLineEdit { border: 1px solid #EAECEF; border-radius: 6px;"
        " padding: 8px 12px; font-size: 13px; background: palette(base); }"
        "QLineEdit:focus { border-color: #2178E5; }");
    layout->addWidget(m_newTodoInput);

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

    // 未完成列表（含分区标题）
    layout->addWidget(createSectionHeader(tr("进行中"), "#2178E5"));
    m_pendingList = new QListWidget(this);
    m_pendingList->setFrameShape(QFrame::NoFrame);
    m_pendingList->setSpacing(2);
    m_pendingList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pendingList->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; }
        QListWidget::item { border-radius: 6px; padding: 0; margin: 1px 0; background: #F8F9FA; }
        QListWidget::item:hover { background: #EEF0F3; }
        QListWidget::item:selected { background: #EEF2FF; }
    )");
    layout->addWidget(m_pendingList, 1);

    // 已完成列表（含分区标题）
    layout->addWidget(createSectionHeader(tr("已完成"), "#52C41A"));
    m_completedList = new QListWidget(this);
    m_completedList->setFrameShape(QFrame::NoFrame);
    m_completedList->setSpacing(2);
    m_completedList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_completedList->setMaximumHeight(200);
    m_completedList->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; }
        QListWidget::item { border-radius: 6px; padding: 0; margin: 1px 0; background: #F5F5F5; }
    )");
    layout->addWidget(m_completedList);

    // 单击待办在右侧编辑器打开
    auto setupClick = [this](QListWidget *list) {
        connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
            int todoId = item->data(Qt::UserRole).toInt();
            if (todoId > 0) emit todoSelected(todoId);
        });
    };
    setupClick(m_pendingList);
    setupClick(m_completedList);

    // 右键菜单
    m_pendingList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_completedList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_pendingList, &QListWidget::customContextMenuRequested, this, &TodoWidget::onContextMenu);
    connect(m_completedList, &QListWidget::customContextMenuRequested, this, &TodoWidget::onContextMenu);
}

void TodoWidget::onContextMenu(const QPoint &pos)
{
    QListWidget *list = qobject_cast<QListWidget *>(sender());
    if (list) showTodoContextMenu(list, pos);
}

void TodoWidget::showTodoContextMenu(QListWidget *list, const QPoint &pos)
{
    QListWidgetItem *item = list->itemAt(pos);
    if (!item) return;
    int todoId = item->data(Qt::UserRole).toInt();
    if (todoId <= 0) return;

    auto *app = ShorthandApplication::instance();
    TodoManager *mgr = app->todoManager();
    TodoData todo = mgr->getTodo(todoId);

    QMenu menu(this);
    // 跟随系统主题（深色/浅色均可读），删除等菜单项选中时使用主题高亮色
    menu.setStyleSheet(R"(
        QMenu { background: palette(window); border: 1px solid palette(mid); border-radius: 6px; padding: 4px; }
        QMenu::item { padding: 6px 24px; border-radius: 4px; font-size: 13px; color: palette(windowText); }
        QMenu::item:selected { background: palette(highlight); color: palette(highlightedText); }
        QMenu::item:disabled { color: palette(placeholderText); }
        QMenu::separator { height: 1px; background: palette(midlight); margin: 4px 8px; }
    )");

    // 完成/取消完成
    QAction *toggleAction = menu.addAction(todo.isCompleted ? tr("✓ 标记为未完成") : tr("✓ 标记完成"));
    connect(toggleAction, &QAction::triggered, this, [this, mgr, todoId, todo]() {
        mgr->toggleComplete(todoId, !todo.isCompleted);
        emit todoStatusChanged();
        refresh();
    });

    menu.addSeparator();

    // 优先级子菜单
    QMenu *priorityMenu = menu.addMenu(tr("优先级"));
    QAction *pNone = priorityMenu->addAction(tr("无"));
    QAction *pLow = priorityMenu->addAction(tr("低"));
    QAction *pMid = priorityMenu->addAction(tr("中"));
    QAction *pHigh = priorityMenu->addAction(tr("高"));
    QList<QAction *> priorityActions = {pNone, pLow, pMid, pHigh};
    for (QAction *a : priorityActions) {
        connect(a, &QAction::triggered, this, [this, mgr, todoId, priorityActions, a]() {
            int p = priorityActions.indexOf(a);
            mgr->setPriority(todoId, p);
            emit todoStatusChanged();
            refresh();
        });
    }

    // 截止日期
    QAction *dueAction = menu.addAction(tr("📅 设置截止日期..."));
    connect(dueAction, &QAction::triggered, this, [this, mgr, todoId]() { setTodoDueDate(todoId); });

    // 标签子菜单
    QMenu *tagMenu = menu.addMenu(tr("设置标签"));
    QList<TagData> allTags = app->tagManager()->getAllTags();
    QStringList curTags = mgr->getTodoTags(todoId);
    if (allTags.isEmpty()) {
        tagMenu->addAction(tr("（暂无标签）"))->setEnabled(false);
    }
    for (const TagData &tag : allTags) {
        QAction *tagAction = tagMenu->addAction(tag.name);
        tagAction->setCheckable(true);
        tagAction->setChecked(curTags.contains(tag.name));
        int tagId = tag.id;
        connect(tagAction, &QAction::triggered, this, [this, mgr, todoId, tagId, tag, curTags](bool checked) {
            QStringList newTags = curTags;
            if (checked) {
                if (!newTags.contains(tag.name)) newTags.append(tag.name);
            } else {
                newTags.removeAll(tag.name);
            }
            mgr->setTodoTags(todoId, newTags);
            Q_UNUSED(tagId)
            emit todoStatusChanged();
            refresh();
        });
    }
    QAction *newTagAction = tagMenu->addAction(tr("➕ 新建标签..."));
    connect(newTagAction, &QAction::triggered, this, [this, app, mgr, todoId]() {
        bool ok = false;
        QString name = QInputDialog::getText(this, tr("新建标签"), tr("标签名称:"), QLineEdit::Normal, "", &ok);
        if (ok && !name.trimmed().isEmpty()) {
            int tagId = app->tagManager()->createTag(name.trimmed());
            if (tagId > 0) {
                QStringList cur = mgr->getTodoTags(todoId);
                if (!cur.contains(name.trimmed())) cur.append(name.trimmed());
                mgr->setTodoTags(todoId, cur);
                emit todoStatusChanged();
                refresh();
            }
        }
    });

    menu.addSeparator();

    // 删除
    QAction *delAction = menu.addAction(tr("🗑 删除"));
    connect(delAction, &QAction::triggered, this, [this, mgr, todoId]() {
        DDialog dlg(this);
        dlg.setTitle(tr("确认删除"));
        dlg.setMessage(tr("确定删除这条待办吗？"));
        dlg.addButton(tr("取消"));
        dlg.addButton(tr("删除"), true, DDialog::ButtonWarning);
        if (dlg.exec() == 1) {
            mgr->deleteTodo(todoId);
            emit todoStatusChanged();
            refresh();
        }
    });

    menu.exec(list->viewport()->mapToGlobal(pos));
}

void TodoWidget::setTodoDueDate(int todoId)
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("设置截止日期"));
    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    QDateEdit *dateEdit = new QDateEdit(&dlg);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDate(QDate::currentDate());
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    layout->addWidget(dateEdit);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        auto *mgr = ShorthandApplication::instance()->todoManager();
        TodoData t = mgr->getTodo(todoId);
        t.dueDatetime = QDateTime(dateEdit->date(), QTime(23, 59, 59)).toSecsSinceEpoch();
        mgr->updateTodo(t);
        emit todoStatusChanged();
        refresh();
    }
}

QWidget *TodoWidget::createTodoRow(const TodoData &todo)
{
    QWidget *card = new QWidget(this);
    QHBoxLayout *hLayout = new QHBoxLayout(card);
    hLayout->setContentsMargins(12, 6, 12, 6);
    hLayout->setSpacing(8);

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
        QListWidgetItem *hint = new QListWidgetItem(tr("暂无待办，在上方输入框输入内容创建"));
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