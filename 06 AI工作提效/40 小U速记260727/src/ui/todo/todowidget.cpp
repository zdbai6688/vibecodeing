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
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QPixmap>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QPalette>
#include <DLabel>
#include <DFontSizeManager>

// ─── 日程视图本地控件 ────────────────────────────────
// 简单待办卡片：完成勾选 + 标题 + 彩色标签徽章，支持拖拽改期
class TodoCard : public QWidget
{
    Q_OBJECT
public:
    explicit TodoCard(const TodoData &todo, QWidget *parent = nullptr)
        : QWidget(parent), m_todoId(todo.id)
    {
        setStyleSheet("TodoCard { background: palette(light); border-radius: 6px; }");
        QHBoxLayout *h = new QHBoxLayout(this);
        h->setContentsMargins(8, 3, 8, 3);
        h->setSpacing(6);

        QCheckBox *cb = new QCheckBox(this);
        cb->setObjectName("cardCheck");
        cb->setChecked(todo.isCompleted);
        cb->setFixedSize(16, 16);
        cb->setFocusPolicy(Qt::NoFocus);
        h->addWidget(cb);

        DLabel *title = new DLabel(todo.title.isEmpty() ? tr("无标题") : todo.title, this);
        title->setWordWrap(true);
        title->setStyleSheet(todo.isCompleted
            ? "font-size: 12px; color: palette(placeholderText); text-decoration: line-through;"
            : "font-size: 12px; color: palette(windowText);");
        h->addWidget(title, 1);

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
                QLabel *badge = new QLabel(tn, this);
                badge->setFixedHeight(16);
                badge->setStyleSheet(QString(
                    "QLabel { background: %1; color: white; font-size: 9px; font-weight: 600;"
                    " border-radius: 8px; padding: 0 5px; }").arg(color));
                h->addWidget(badge);
            }
        }

        connect(cb, &QCheckBox::toggled, this, [this](bool checked) {
            emit completedToggled(m_todoId, checked);
        });
    }

    int todoId() const { return m_todoId; }

signals:
    void completedToggled(int todoId, bool checked);

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) m_startPos = event->pos();
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!(event->buttons() & Qt::LeftButton)
            || (event->pos() - m_startPos).manhattanLength() < QApplication::startDragDistance()) {
            QWidget::mouseMoveEvent(event);
            return;
        }
        QDrag *drag = new QDrag(this);
        QMimeData *mime = new QMimeData();
        mime->setData(QStringLiteral("application/x-ustodo-card"), QByteArray::number(m_todoId));
        drag->setMimeData(mime);
        QPixmap pm = grab();
        drag->setPixmap(pm);
        drag->setHotSpot(QPoint(pm.width() / 2, 10));
        drag->exec(Qt::MoveAction);
    }

private:
    int m_todoId;
    QPoint m_startPos;
};

// 可接收待办卡片的日期列（dayIndex: 0~6 = 周一~周日，-1 = 未安排列）
class TodoDayList : public QListWidget
{
    Q_OBJECT
public:
    explicit TodoDayList(int dayIndex, QWidget *parent = nullptr)
        : QListWidget(parent), m_dayIndex(dayIndex)
    {
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        setDefaultDropAction(Qt::MoveAction);
    }

signals:
    void dropTodo(int todoId, int dayIndex);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasFormat(QStringLiteral("application/x-ustodo-card")))
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->mimeData()->hasFormat(QStringLiteral("application/x-ustodo-card")))
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void dropEvent(QDropEvent *event) override
    {
        if (!event->mimeData()->hasFormat(QStringLiteral("application/x-ustodo-card"))) {
            event->ignore();
            return;
        }
        bool ok = false;
        int todoId = event->mimeData()->data(QStringLiteral("application/x-ustodo-card")).toInt(&ok);
        if (ok && todoId > 0) {
            emit dropTodo(todoId, m_dayIndex);
            event->acceptProposedAction();
        } else {
            event->ignore();
        }
    }

private:
    int m_dayIndex;
};

// 待办列表：每条显示标题+创建时间+标签+复选框，未完成/已完成分区
TodoWidget::TodoWidget(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("TodoWidget { background: palette(base); }");
    QDate today = QDate::currentDate();
    m_calendarMonday = today.addDays(-(int)today.dayOfWeek() + 1);
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
        "QLineEdit { border: 1px solid palette(mid); border-radius: 6px;"
        " padding: 8px 12px; font-size: 13px; background: palette(base); }"
        "QLineEdit:focus { border-color: palette(highlight); }");
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

    // 日程/列表视图切换按钮
    m_calendarToggleBtn = new QPushButton(tr("🗓 日程视图"), this);
    m_calendarToggleBtn->setCheckable(true);
    m_calendarToggleBtn->setFixedHeight(36);
    m_calendarToggleBtn->setFixedWidth(96);
    m_calendarToggleBtn->setStyleSheet(
        "QPushButton { border: 1px solid palette(mid); border-radius: 6px;"
        " padding: 4px 8px; font-size: 12px; background: palette(base); }"
        "QPushButton:hover { background: palette(light); }"
        "QPushButton:checked { background: palette(highlight); color: white; border-color: palette(highlight); }");
    topRow->addWidget(m_calendarToggleBtn);

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
    m_pendingHeader = createSectionHeader(tr("进行中"), "palette(highlight)");
    layout->addWidget(m_pendingHeader);
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
    m_completedHeader = createSectionHeader(tr("已完成"), "palette(placeholderText)");
    layout->addWidget(m_completedHeader);
    m_completedList = new QListWidget(this);
    m_completedList->setFrameShape(QFrame::NoFrame);
    m_completedList->setSpacing(4);
    m_completedList->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; }
        QListWidget::item { border-radius: 8px; background: palette(light); margin: 2px 0; }
        QListWidget::item:hover { background: palette(midlight); }
    )");
    layout->addWidget(m_completedList, 1);

    // ─── 日程网格视图（默认隐藏，切换按钮开启） ──
    layout->addWidget(m_calendarView, 1);

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
    m_pendingList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_completedList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_pendingList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        showTodoContextMenu(m_pendingList, pos);
    });
    connect(m_completedList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        showTodoContextMenu(m_completedList, pos);
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

    // 日程/列表视图切换
    connect(m_calendarToggleBtn, &QPushButton::toggled, this, [this](bool on) {
        setCalendarMode(on);
    });

    initCalendarView();
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

    QAction *dueAction = menu.addAction(tr("设置截止日期"));
    connect(dueAction, &QAction::triggered, this, [this, todoId]() { setTodoDueDate(todoId); });

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
        ? "font-size: 13px; color: palette(placeholderText); text-decoration: line-through;"
        : "font-size: 13px; color: palette(windowText);");
    tl->addWidget(titleLabel);

    // 创建时间 + 截止日期
    QString timeStr = tr("创建于 ") + todo.createdAt().toString("MM-dd HH:mm");
    if (todo.dueDatetime > 0) {
        timeStr += tr("  截止 ") + QDateTime::fromSecsSinceEpoch(todo.dueDatetime).toString("MM-dd");
    }
    DLabel *timeLabel = new DLabel(timeStr, card);
    timeLabel->setStyleSheet("font-size: 10px; color: palette(placeholderText);");
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
        hint->setForeground(palette().color(QPalette::PlaceholderText));
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

    if (m_calendarMode) {
        populateCalendarView(all);
    } else {
        populateList(all);
    }
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

void TodoWidget::clearFilterTags()
{
    m_filterTags.clear();
}

void TodoWidget::focusNewTodoInput()
{
    m_newTodoInput->setFocus();
    m_newTodoInput->selectAll();
}

// ─── 日程网格视图（Phase D） ──────────────────────────
void TodoWidget::setCalendarMode(bool on)
{
    m_calendarMode = on;
    if (m_calendarToggleBtn) {
        m_calendarToggleBtn->setText(on ? tr("☰ 列表视图") : tr("🗓 日程视图"));
    }
    if (m_pendingHeader) m_pendingHeader->setVisible(!on);
    if (m_pendingList) m_pendingList->setVisible(!on);
    if (m_completedHeader) m_completedHeader->setVisible(!on);
    if (m_completedList) m_completedList->setVisible(!on);
    if (m_calendarView) m_calendarView->setVisible(on);
    if (m_selectModeBtn) m_selectModeBtn->setVisible(!on);
    if (m_batchToolbar) m_batchToolbar->setVisible(!on && m_multiSelectMode);
    refresh();
}

void TodoWidget::initCalendarView()
{
    m_calendarView = new QWidget(this);
    QVBoxLayout *calLayout = new QVBoxLayout(m_calendarView);
    calLayout->setContentsMargins(0, 0, 0, 0);
    calLayout->setSpacing(8);

    // 周导航
    QHBoxLayout *navRow = new QHBoxLayout();
    navRow->setSpacing(8);
    m_calendarPrevBtn = new QPushButton(tr("◀ 上一周"), m_calendarView);
    m_calendarPrevBtn->setFixedHeight(30);
    m_calendarPrevBtn->setStyleSheet(
        "QPushButton { background: transparent; border: 1px solid palette(mid); border-radius: 6px;"
        " padding: 2px 12px; font-size: 12px; color: palette(windowText); }"
        "QPushButton:hover { border-color: palette(highlight); color: palette(highlight); }");
    m_calendarNextBtn = new QPushButton(tr("下一周 ▶"), m_calendarView);
    m_calendarNextBtn->setFixedHeight(30);
    m_calendarNextBtn->setStyleSheet(m_calendarPrevBtn->styleSheet());
    m_calendarWeekLabel = new QLabel(m_calendarView);
    m_calendarWeekLabel->setAlignment(Qt::AlignCenter);
    m_calendarWeekLabel->setStyleSheet("font-size: 13px; font-weight: 700; color: palette(windowText);");
    navRow->addWidget(m_calendarPrevBtn);
    navRow->addWidget(m_calendarWeekLabel, 1);
    navRow->addWidget(m_calendarNextBtn);
    calLayout->addLayout(navRow);

    connect(m_calendarPrevBtn, &QPushButton::clicked, this, &TodoWidget::onCalendarPrevWeek);
    connect(m_calendarNextBtn, &QPushButton::clicked, this, &TodoWidget::onCalendarNextWeek);

    // 7 列网格：周一~周日
    QStringList dayNames = {tr("周一"), tr("周二"), tr("周三"), tr("周四"), tr("周五"), tr("周六"), tr("周日")};
    QHBoxLayout *gridLayout = new QHBoxLayout();
    gridLayout->setSpacing(6);
    for (int i = 0; i < 7; ++i) {
        QWidget *col = new QWidget(m_calendarView);
        QVBoxLayout *colLayout = new QVBoxLayout(col);
        colLayout->setContentsMargins(0, 0, 0, 0);
        colLayout->setSpacing(4);

        DLabel *weekday = new DLabel(dayNames[i], col);
        weekday->setAlignment(Qt::AlignCenter);
        weekday->setStyleSheet("font-size: 11px; color: palette(placeholderText);");
        colLayout->addWidget(weekday);

        QLabel *dateNum = new QLabel(col);
        dateNum->setAlignment(Qt::AlignCenter);
        dateNum->setFixedSize(26, 26);
        dateNum->setStyleSheet("font-size: 11px; font-weight: 600; color: palette(windowText); border-radius: 13px;");
        colLayout->addWidget(dateNum, 0, Qt::AlignHCenter);
        m_calendarDateLabels.append(dateNum);

        TodoDayList *list = new TodoDayList(i, col);
        list->setFrameShape(QFrame::NoFrame);
        list->setSpacing(2);
        list->setStyleSheet(R"(
            QListWidget { background: palette(alternateBase); border: 1px solid palette(mid); border-radius: 6px; padding: 2px; }
            QListWidget::item { border-radius: 6px; background: transparent; margin: 1px 0; }
            QListWidget::item:selected { background: palette(highlight); }
        )");
        connect(list, &TodoDayList::dropTodo, this, [this](int todoId, int dayIndex) {
            onTodoDropped(todoId, dayIndex);
        });
        colLayout->addWidget(list, 1);
        m_dayLists.append(list);
        gridLayout->addWidget(col, 1);
    }
    calLayout->addLayout(gridLayout, 1);

    // 未安排/其他周列表
    DLabel *unschedTitle = new DLabel(tr("📌 未安排 / 其他周"), m_calendarView);
    unschedTitle->setStyleSheet("font-size: 11px; font-weight: 600; color: palette(placeholderText);");
    calLayout->addWidget(unschedTitle);

    m_unscheduledList = new TodoDayList(-1, m_calendarView);
    m_unscheduledList->setFrameShape(QFrame::NoFrame);
    m_unscheduledList->setSpacing(2);
    m_unscheduledList->setStyleSheet(
        "QListWidget { background: palette(alternateBase); border: 1px dashed palette(mid); border-radius: 6px; padding: 2px; }"
        "QListWidget::item { border-radius: 6px; background: transparent; margin: 1px 0; }"
        "QListWidget::item:selected { background: palette(highlight); }");
    m_unscheduledList->setFixedHeight(150);
    connect(qobject_cast<TodoDayList *>(m_unscheduledList), &TodoDayList::dropTodo, this,
            [this](int todoId, int dayIndex) {
                onTodoDropped(todoId, dayIndex);
            });
    calLayout->addWidget(m_unscheduledList);

    m_calendarView->hide();
}

void TodoWidget::updateCalendarWeekLabel()
{
    if (!m_calendarWeekLabel || !m_calendarNextBtn) return;
    QDate sunday = m_calendarMonday.addDays(6);
    m_calendarWeekLabel->setText(QString("%1 ~ %2").arg(
        m_calendarMonday.toString("yyyy-MM-dd"), sunday.toString("yyyy-MM-dd")));
    QDate thisMonday = QDate::currentDate().addDays(-(int)QDate::currentDate().dayOfWeek() + 1);
    m_calendarNextBtn->setEnabled(m_calendarMonday < thisMonday);
}

void TodoWidget::populateCalendarView(const QList<TodoData> &todos)
{
    for (QListWidget *list : m_dayLists) list->clear();
    m_unscheduledList->clear();

    QDate today = QDate::currentDate();
    for (int i = 0; i < 7; ++i) {
        QLabel *label = m_calendarDateLabels.value(i);
        if (!label) continue;
        QDate d = m_calendarMonday.addDays(i);
        label->setText(QString::number(d.day()));
        const bool isToday = (d == today);
        label->setStyleSheet(isToday
            ? "font-size: 11px; font-weight: 700; color: white; background: palette(highlight); border-radius: 13px;"
            : "font-size: 11px; font-weight: 600; color: palette(windowText); border-radius: 13px;");
    }
    updateCalendarWeekLabel();

    const QDate weekEnd = m_calendarMonday.addDays(6);
    for (const TodoData &t : todos) {
        QListWidget *target = nullptr;
        if (t.dueDatetime > 0) {
            QDate due = QDateTime::fromSecsSinceEpoch(t.dueDatetime).date();
            if (due >= m_calendarMonday && due <= weekEnd) {
                target = m_dayLists.value(m_calendarMonday.daysTo(due));
            }
        }
        if (!target) target = m_unscheduledList;

        QWidget *card = createTodoCard(t);
        QListWidgetItem *item = new QListWidgetItem(target);
        item->setData(Qt::UserRole, t.id);
        item->setSizeHint(QSize(0, 46));
        target->setItemWidget(item, card);
    }

    // 空列提示
    auto addEmptyHint = [this](QListWidget *list, const QString &text) {
        if (list->count() == 0) {
            QListWidgetItem *hint = new QListWidgetItem(text, list);
            hint->setFlags(Qt::NoItemFlags);
            hint->setForeground(palette().color(QPalette::PlaceholderText));
            hint->setTextAlignment(Qt::AlignCenter);
        }
    };
    for (QListWidget *list : m_dayLists) addEmptyHint(list, tr("—"));
    addEmptyHint(m_unscheduledList, tr("拖到此处清除截止日期"));
}

void TodoWidget::onCalendarPrevWeek()
{
    m_calendarMonday = m_calendarMonday.addDays(-7);
    refresh();
}

void TodoWidget::onCalendarNextWeek()
{
    QDate nextMonday = m_calendarMonday.addDays(7);
    QDate thisMonday = QDate::currentDate().addDays(-(int)QDate::currentDate().dayOfWeek() + 1);
    if (nextMonday <= thisMonday) {
        m_calendarMonday = nextMonday;
        refresh();
    }
}

QWidget *TodoWidget::createTodoCard(const TodoData &todo)
{
    TodoCard *card = new TodoCard(todo, this);
    connect(card, &TodoCard::completedToggled, this, [this](int todoId, bool checked) {
        auto *app = ShorthandApplication::instance();
        if (app && app->todoManager()) {
            app->todoManager()->toggleComplete(todoId, checked);
            emit todoStatusChanged();
        }
        refresh();
    });
    return card;
}

void TodoWidget::onTodoDropped(int todoId, int dayIndex)
{
    auto *app = ShorthandApplication::instance();
    if (!app || !app->todoManager()) return;
    TodoData todo = app->todoManager()->getTodo(todoId);
    if (todo.id <= 0) return;

    if (dayIndex >= 0 && dayIndex < 7) {
        QDate targetDate = m_calendarMonday.addDays(dayIndex);
        todo.dueDatetime = QDateTime(targetDate, QTime(23, 59, 59)).toSecsSinceEpoch();
    } else {
        todo.dueDatetime = 0; // 拖到未安排列：清除截止日期
    }
    todo.modificationDatetime = QDateTime::currentSecsSinceEpoch();
    app->todoManager()->updateTodo(todo);
    refresh();
}

#include "todowidget.moc"
