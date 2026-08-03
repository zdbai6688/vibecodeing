// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "weeklyreportwidget.h"
#include "application/shorthandapplication.h"
#include "core/todomanager.h"
#include "core/notemanager.h"
#include "services/aiservice.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <DLabel>
#include <DFontSizeManager>
#include <DDialog>
#include <QDebug>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QInputDialog>
#include <QListWidgetItem>
#include <QMap>
#include <QMouseEvent>

DWIDGET_USE_NAMESPACE

class ClickableDayCell : public QWidget
{
    Q_OBJECT
public:
    explicit ClickableDayCell(int dayIndex, QWidget *parent = nullptr)
        : QWidget(parent), m_dayIndex(dayIndex) {}

signals:
    void clicked(int dayIndex);
    void doubleClicked(int dayIndex);

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            emit clicked(m_dayIndex);
        }
        QWidget::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            emit doubleClicked(m_dayIndex);
        }
        QWidget::mouseDoubleClickEvent(event);
    }

private:
    int m_dayIndex;
};

WeeklyReportWidget::WeeklyReportWidget(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("WeeklyReportWidget { background: palette(base); }");
    QDate today = QDate::currentDate();
    m_currentMonday = today.addDays(-(int)today.dayOfWeek() + 1);
    initUI();
    refresh();
}

void WeeklyReportWidget::initUI()
{
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    QWidget *container = new QWidget(scroll);
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(16);

    // 头部导航
    QHBoxLayout *navRow = new QHBoxLayout();
    m_prevBtn = new QPushButton(tr("◀ 上一周"), this);
    m_prevBtn->setFixedHeight(32);
    m_prevBtn->setStyleSheet("QPushButton { background:transparent; border:1px solid #EAECEF; border-radius:6px; padding:4px 14px; font-size:13px; color:#666; } QPushButton:hover { border-color:#2178E5; color:#2178E5; }");
    m_nextBtn = new QPushButton(tr("下一周 ▶"), this);
    m_nextBtn->setFixedHeight(32);
    m_nextBtn->setStyleSheet("QPushButton { background:transparent; border:1px solid #EAECEF; border-radius:6px; padding:4px 14px; font-size:13px; color:#666; } QPushButton:hover { border-color:#2178E5; color:#2178E5; }");
    m_weekLabel = new DLabel(this);
    m_weekLabel->setStyleSheet("font-size: 16px; font-weight: 700; color: #222;");
    m_weekLabel->setAlignment(Qt::AlignCenter);
    navRow->addWidget(m_prevBtn);
    navRow->addWidget(m_weekLabel, 1);
    navRow->addWidget(m_nextBtn);
    mainLayout->addLayout(navRow);

    // 日历卡片
    QWidget *calendarCard = new QWidget(this);
    calendarCard->setStyleSheet("background: #F8F9FA; border-radius: 8px;");
    QVBoxLayout *calendarCardLayout = new QVBoxLayout(calendarCard);
    calendarCardLayout->setContentsMargins(16, 16, 16, 16);
    calendarCardLayout->setSpacing(12);

    DLabel *calendarTitle = new DLabel(tr("📅 本周日程"), this);
    calendarTitle->setStyleSheet("font-size: 14px; font-weight: 600; color: #222;");
    calendarCardLayout->addWidget(calendarTitle);

    QHBoxLayout *calendarLayout = new QHBoxLayout();
    calendarLayout->setSpacing(8);

    QStringList dayNames = {tr("周一"), tr("周二"), tr("周三"), tr("周四"), tr("周五"), tr("周六"), tr("周日")};
    for (int i = 0; i < 7; i++) {
        ClickableDayCell *dayCell = new ClickableDayCell(i, this);
        dayCell->setStyleSheet("QWidget { background: #FFFFFF; border-radius: 8px; } QWidget:hover { background: #EEF2FF; }");
        QVBoxLayout *cellLayout = new QVBoxLayout(dayCell);
        cellLayout->setContentsMargins(8, 12, 8, 12);
        cellLayout->setSpacing(6);
        cellLayout->setAlignment(Qt::AlignCenter);

        m_weekdayLabels[i] = new DLabel(dayNames[i], dayCell);
        m_weekdayLabels[i]->setAlignment(Qt::AlignCenter);
        m_weekdayLabels[i]->setStyleSheet("font-size: 11px; color: #999;");

        m_weekdayNums[i] = new DLabel(dayCell);
        m_weekdayNums[i]->setAlignment(Qt::AlignCenter);
        m_weekdayNums[i]->setFixedSize(34, 34);
        m_weekdayNums[i]->setStyleSheet("font-size: 15px; font-weight: 700; color: #222; border-radius: 17px;");

        m_dayTodoLabels[i] = new DLabel(dayCell);
        m_dayTodoLabels[i]->setAlignment(Qt::AlignCenter);
        m_dayTodoLabels[i]->setWordWrap(true);
        m_dayTodoLabels[i]->setStyleSheet("font-size: 10px; color: #2178E5; line-height: 1.4;");
        m_dayTodoLabels[i]->setMaximumHeight(44);

        cellLayout->addWidget(m_weekdayLabels[i]);
        cellLayout->addWidget(m_weekdayNums[i]);
        cellLayout->addWidget(m_dayTodoLabels[i]);

        connect(dayCell, &ClickableDayCell::doubleClicked, this, [this](int dayIndex) {
            onDayCellDoubleClicked(dayIndex);
        });
        connect(dayCell, &ClickableDayCell::clicked, this, &WeeklyReportWidget::onDayClicked);

        m_dayCells[i] = dayCell;
        calendarLayout->addWidget(dayCell, 1);
    }
    calendarCardLayout->addLayout(calendarLayout);

    DLabel *hintLabel = new DLabel(tr("💡 单击日期查看该日待办，双击可快速创建该日待办"), this);
    hintLabel->setStyleSheet("font-size: 11px; color: #999;");
    hintLabel->setAlignment(Qt::AlignCenter);
    calendarCardLayout->addWidget(hintLabel);
    mainLayout->addWidget(calendarCard);

    // 当日待办卡片
    QWidget *dayTodoCard = new QWidget(this);
    dayTodoCard->setStyleSheet("background: #F8F9FA; border-radius: 8px;");
    QVBoxLayout *dayTodoCardLayout = new QVBoxLayout(dayTodoCard);
    dayTodoCardLayout->setContentsMargins(16, 12, 16, 12);
    dayTodoCardLayout->setSpacing(8);

    m_dayTodoTitle = new QLabel(tr("📅 点击上方日期查看该日待办"), this);
    m_dayTodoTitle->setStyleSheet("font-size: 13px; font-weight: 600; color: #222;");
    dayTodoCardLayout->addWidget(m_dayTodoTitle);

    m_dayTodoList = new QListWidget(this);
    m_dayTodoList->setMaximumHeight(140);
    m_dayTodoList->setFrameShape(QFrame::NoFrame);
    m_dayTodoList->setStyleSheet(
        "QListWidget { background: transparent; border: none; }"
        "QListWidget::item { font-size: 13px; color: #222; padding: 6px 8px; border-radius: 4px; }");
    m_dayTodoList->setAlternatingRowColors(true);
    dayTodoCardLayout->addWidget(m_dayTodoList);
    mainLayout->addWidget(dayTodoCard);

    // 统计卡片
    QWidget *statsCard = new QWidget(this);
    statsCard->setStyleSheet("background: #F8F9FA; border-radius: 8px;");
    QVBoxLayout *statsCardLayout = new QVBoxLayout(statsCard);
    statsCardLayout->setContentsMargins(16, 12, 16, 12);
    statsCardLayout->setSpacing(8);

    DLabel *statsTitle = new DLabel(tr("📊 本周统计"), this);
    statsTitle->setStyleSheet("font-size: 14px; font-weight: 600; color: #222;");
    statsCardLayout->addWidget(statsTitle);

    QHBoxLayout *statsRow = new QHBoxLayout();
    statsRow->setSpacing(16);

    m_rateLabel = new DLabel(this);
    m_rateLabel->setStyleSheet("font-size: 22px; font-weight: 700; color: #2178E5;");
    statsRow->addWidget(m_rateLabel);

    QVBoxLayout *statsDetail = new QVBoxLayout();
    statsDetail->setSpacing(4);
    m_totalLabel = new DLabel(this);
    m_totalLabel->setStyleSheet("font-size: 12px; color: #666;");
    m_completedLabel = new DLabel(this);
    m_completedLabel->setStyleSheet("font-size: 12px; color: #52C41A;");
    m_pendingLabel = new DLabel(this);
    m_pendingLabel->setStyleSheet("font-size: 12px; color: #FAAD14;");
    m_overdueLabel = new DLabel(this);
    m_overdueLabel->setStyleSheet("font-size: 12px; color: #E64545;");
    statsDetail->addWidget(m_totalLabel);
    statsDetail->addWidget(m_completedLabel);
    statsDetail->addWidget(m_pendingLabel);
    statsDetail->addWidget(m_overdueLabel);
    statsRow->addLayout(statsDetail, 1);
    statsCardLayout->addLayout(statsRow);

    m_tagStatsLabel = new DLabel(this);
    m_tagStatsLabel->setStyleSheet("font-size: 11px; color: #999;");
    m_tagStatsLabel->setWordWrap(true);
    statsCardLayout->addWidget(m_tagStatsLabel);
    mainLayout->addWidget(statsCard);

    // 事项列表卡片
    QWidget *listCard = new QWidget(this);
    listCard->setStyleSheet("background: #F8F9FA; border-radius: 8px;");
    QVBoxLayout *listCardLayout = new QVBoxLayout(listCard);
    listCardLayout->setContentsMargins(16, 12, 16, 12);
    listCardLayout->setSpacing(10);

    DLabel *pendingTitle = new DLabel(tr("📋 未完成事项"), this);
    pendingTitle->setStyleSheet("font-size: 13px; font-weight: 600; color: #222;");
    listCardLayout->addWidget(pendingTitle);
    m_pendingList = new QListWidget(this);
    m_pendingList->setMaximumHeight(120);
    m_pendingList->setFrameShape(QFrame::NoFrame);
    m_pendingList->setStyleSheet("QListWidget { background: transparent; border: none; } QListWidget::item { font-size: 12px; color: #666; padding: 4px 8px; }");
    listCardLayout->addWidget(m_pendingList);

    DLabel *completedTitle = new DLabel(tr("✅ 已完成事项"), this);
    completedTitle->setStyleSheet("font-size: 13px; font-weight: 600; color: #222;");
    listCardLayout->addWidget(completedTitle);
    m_completedList = new QListWidget(this);
    m_completedList->setMaximumHeight(120);
    m_completedList->setFrameShape(QFrame::NoFrame);
    m_completedList->setStyleSheet("QListWidget { background: transparent; border: none; } QListWidget::item { font-size: 12px; color: #999; padding: 4px 8px; }");
    listCardLayout->addWidget(m_completedList);
    mainLayout->addWidget(listCard);

    // 周报预览卡片
    QWidget *previewCard = new QWidget(this);
    previewCard->setStyleSheet("background: #F8F9FA; border-radius: 8px;");
    QVBoxLayout *previewCardLayout = new QVBoxLayout(previewCard);
    previewCardLayout->setContentsMargins(16, 12, 16, 12);
    previewCardLayout->setSpacing(8);

    DLabel *previewTitle = new DLabel(tr("📝 周报预览"), this);
    previewTitle->setStyleSheet("font-size: 13px; font-weight: 600; color: #222;");
    previewCardLayout->addWidget(previewTitle);

    m_reportPreview = new QTextEdit(this);
    m_reportPreview->setMinimumHeight(160);
    m_reportPreview->setStyleSheet("QTextEdit { background: #FFFFFF; border: 1px solid #EAECEF; border-radius: 6px; padding: 10px; font-size: 12px; color: #222; }");
    m_reportPreview->setPlaceholderText(tr("点击「AI 生成周报」按钮，将基于本周待办数据生成周报..."));
    previewCardLayout->addWidget(m_reportPreview);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_generateBtn = new QPushButton(tr("🤖 AI 生成周报"), this);
    m_generateBtn->setFixedHeight(36);
    m_generateBtn->setStyleSheet("QPushButton { background:#2178E5; color:white; border:none; border-radius:6px; padding:6px 20px; font-size:13px; font-weight:600; } QPushButton:hover { background:#1A6AD4; } QPushButton:disabled { background:#CCCCCC; }");
    m_exportBtn = new QPushButton(tr("📤 导出"), this);
    m_exportBtn->setFixedHeight(36);
    m_exportBtn->setStyleSheet("QPushButton { background:transparent; border:1px solid #2178E5; border-radius:6px; padding:6px 16px; font-size:12px; color:#2178E5; } QPushButton:hover { background:#F0F5FF; }");
    btnRow->addWidget(m_generateBtn);
    btnRow->addWidget(m_exportBtn);
    previewCardLayout->addLayout(btnRow);
    mainLayout->addWidget(previewCard);

    mainLayout->addStretch();
    scroll->setWidget(container);
    outerLayout->addWidget(scroll);

    connect(m_prevBtn, &QPushButton::clicked, this, &WeeklyReportWidget::onPrevWeek);
    connect(m_nextBtn, &QPushButton::clicked, this, &WeeklyReportWidget::onNextWeek);
    connect(m_generateBtn, &QPushButton::clicked, this, &WeeklyReportWidget::onGenerateReport);
    connect(m_exportBtn, &QPushButton::clicked, this, &WeeklyReportWidget::onExportReport);
}



void WeeklyReportWidget::onDayClicked(int dayIndex)
{
    if (dayIndex < 0 || dayIndex >= 7) return;
    QDate d = m_currentMonday.addDays(dayIndex);
    m_selectedDate = d;
    updateDayTodoList(d);

    for (int i = 0; i < 7; i++) {
        m_dayCells[i]->setStyleSheet(i == dayIndex
            ? "QWidget { background: #D6E6FF; border-radius: 8px; }"
            : "QWidget { background: #FFFFFF; border-radius: 8px; } QWidget:hover { background: #EEF2FF; }");
    }
}

void WeeklyReportWidget::updateDayTodoList(const QDate &date)
{
    m_dayTodoTitle->setText(tr("📅 %1 的待办").arg(date.toString("yyyy-MM-dd")));
    m_dayTodoList->clear();

    auto *mgr = ShorthandApplication::instance()->todoManager();
    qint64 dayStart = QDateTime(date, QTime(0, 0)).toSecsSinceEpoch();
    qint64 dayEnd = QDateTime(date, QTime(23, 59, 59)).toSecsSinceEpoch();
    QList<TodoData> allTodos = mgr->getAllTodos();

    int count = 0;
    for (const auto &t : allTodos) {
        qint64 match = t.dueDatetime;
        if (match == 0) match = t.creationDatetime;
        if (match >= dayStart && match <= dayEnd) {
            QString text = (t.isCompleted ? "☑ " : "☐ ") + t.title;
            QListWidgetItem *item = new QListWidgetItem(text);
            if (t.isCompleted) item->setForeground(QColor("#BBBBBB"));
            m_dayTodoList->addItem(item);
            count++;
        }
    }
    if (count == 0) {
        QListWidgetItem *empty = new QListWidgetItem(tr("该日暂无待办"));
        empty->setForeground(QColor("#999999"));
        m_dayTodoList->addItem(empty);
    }
}

void WeeklyReportWidget::onDayCellDoubleClicked(int dayIndex)
{
    if (dayIndex < 0 || dayIndex >= 7) return;

    QDate clickedDate = m_currentMonday.addDays(dayIndex);
    QDate thisMonday = QDate::currentDate().addDays(-(int)QDate::currentDate().dayOfWeek() + 1);
    if (m_currentMonday > thisMonday) return;

    DDialog dlg(this);
    dlg.setTitle(tr("快速创建待办"));
    dlg.setFixedSize(380, 200);

    QWidget *widget = new QWidget(&dlg);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(12);
    layout->setContentsMargins(16, 8, 16, 8);

    QLineEdit *titleEdit = new QLineEdit(widget);
    titleEdit->setPlaceholderText(tr("输入待办内容"));
    titleEdit->selectAll();
    layout->addWidget(new DLabel(tr("待办内容:"), widget));
    layout->addWidget(titleEdit);

    QHBoxLayout *dateRow = new QHBoxLayout();
    DLabel *dateLabel = new DLabel(tr("截止日期:"), widget);
    dateRow->addWidget(dateLabel);

    DLabel *dateValue = new DLabel(clickedDate.toString("yyyy-MM-dd") + " (" + tr("双击的日期") + ")", widget);
    dateValue->setStyleSheet("color: palette(highlight); font-weight: 600;");
    dateRow->addWidget(dateValue);
    dateRow->addStretch();
    layout->addLayout(dateRow);

    dlg.addContent(widget);

    int cancelBtn = dlg.addButton(tr("取消"), false, DDialog::ButtonNormal);
    int okBtn = dlg.addButton(tr("创建"), true, DDialog::ButtonRecommend);
    Q_UNUSED(cancelBtn);

    if (dlg.exec() == okBtn) {
        QString text = titleEdit->text().trimmed();
        if (text.isEmpty()) return;

        auto *mgr = ShorthandApplication::instance()->todoManager();
        TodoData todo;
        todo.title = text;
        todo.dueDatetime = QDateTime(clickedDate, QTime(23, 59, 59)).toSecsSinceEpoch();
        todo.creationDatetime = QDateTime::currentSecsSinceEpoch();
        todo.modificationDatetime = todo.creationDatetime;

        mgr->createTodo(todo);
        refresh();
    }
}

void WeeklyReportWidget::updateCalendarCells()
{
    QDate today = QDate::currentDate();
    for (int i = 0; i < 7; i++) {
        QDate d = m_currentMonday.addDays(i);
        bool isClickable = (d <= today);
        m_dayCells[i]->setCursor(isClickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
        m_dayCells[i]->setToolTip(isClickable
            ? tr("双击创建 %1 的待办").arg(d.toString("MM-dd"))
            : "");
    }
}

QString WeeklyReportWidget::formatWeekDate(const QDate &date)
{
    QDate end = date.addDays(6);
    return QString("%1 ~ %2").arg(date.toString("M/dd"), end.toString("M/dd"));
}

void WeeklyReportWidget::refresh()
{
    QDate today = QDate::currentDate();
    QDate thisMonday = today.addDays(-(int)today.dayOfWeek() + 1);

    m_weekLabel->setText(formatWeekDate(m_currentMonday));
    // 如果当前周是本周，禁用「下一周」按钮
    bool isCurrentWeek = (m_currentMonday == thisMonday);
    m_nextBtn->setEnabled(!isCurrentWeek);

    // 更新日历
    for (int i = 0; i < 7; i++) {
        QDate d = m_currentMonday.addDays(i);
        m_weekdayNums[i]->setText(QString::number(d.day()));
        bool isToday = (d == today);
        m_weekdayNums[i]->setStyleSheet(QString(
            "font-size: 14px; font-weight: 600; border-radius: 16px; %1"
        ).arg(isToday
            ? "background: palette(highlight); color: palette(highlightedText);"
            : ""));
    }

    // 计算统计数据
    auto *app = ShorthandApplication::instance();
    auto *todoMgr = app->todoManager();
    qint64 weekStart = QDateTime(m_currentMonday, QTime(0, 0)).toSecsSinceEpoch();
    qint64 weekEnd = QDateTime(m_currentMonday.addDays(6), QTime(23, 59, 59)).toSecsSinceEpoch();

    int total = 0, completed = 0, pending = 0, overdue = 0;
    QStringList completedItems, pendingItems;
    QList<TodoData> allTodos = todoMgr->getAllTodos();
    QList<TodoData> completedTodos = todoMgr->getCompletedTodos();

    for (const auto &todo : allTodos) {
        if (todo.dueDatetime >= weekStart && todo.dueDatetime <= weekEnd) {
            total++;
            if (todo.isCompleted) {
                completed++;
                completedItems << "✅ " + todo.title;
            } else {
                pending++;
                pendingItems << "⏳ " + todo.title;
                if (todo.isOverdue()) overdue++;
            }
        }
    }
    for (const auto &todo : completedTodos) {
        if (todo.completedDatetime >= weekStart && todo.completedDatetime <= weekEnd) {
            bool found = false;
            for (const auto &t : allTodos) { if (t.id == todo.id) { found = true; break; } }
            if (!found) { total++; completed++; completedItems << "✅ " + todo.title; }
        }
    }

    double rate = total > 0 ? (double)completed / total * 100 : 0;

    m_totalLabel->setText(tr("📋 本周共计：%1 项").arg(total));
    m_completedLabel->setText(tr("✅ 已完成：%1").arg(completed));
    m_pendingLabel->setText(tr("⏳ 未完成：%1").arg(pending));
    m_overdueLabel->setText(tr("⚠️ 逾期：%1").arg(overdue));
    m_rateLabel->setText(tr("完成率 %1%").arg(QString::number(rate, 'f', 1)));

    m_completedList->clear();
    if (completedItems.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem(tr("本周无已完成事项"));
        item->setForeground(QColor(palette().color(QPalette::PlaceholderText)));
        m_completedList->addItem(item);
    } else {
        for (const auto &item : completedItems) m_completedList->addItem(item);
    }

    m_pendingList->clear();
    if (pendingItems.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem(tr("本周无未完成事项"));
        item->setForeground(QColor(palette().color(QPalette::PlaceholderText)));
        m_pendingList->addItem(item);
    } else {
        for (const auto &item : pendingItems) m_pendingList->addItem(item);
    }
}

void WeeklyReportWidget::onPrevWeek() { m_currentMonday = m_currentMonday.addDays(-7); refresh(); }
void WeeklyReportWidget::onNextWeek()
{
    QDate nextMonday = m_currentMonday.addDays(7);
    QDate thisMonday = QDate::currentDate().addDays(-(int)QDate::currentDate().dayOfWeek() + 1);
    if (nextMonday <= thisMonday) { m_currentMonday = nextMonday; refresh(); }
}

QString WeeklyReportWidget::buildReportContent()
{
    QDate sunday = m_currentMonday.addDays(6);
    auto *app = ShorthandApplication::instance();
    auto *todoMgr = app->todoManager();
    qint64 weekStart = QDateTime(m_currentMonday, QTime(0, 0)).toSecsSinceEpoch();
    qint64 weekEnd = QDateTime(sunday, QTime(23, 59, 59)).toSecsSinceEpoch();

    int total = 0, completed = 0;
    QStringList completedItems, pendingItems;
    QList<TodoData> allTodos = todoMgr->getAllTodos();
    QList<TodoData> completedTodos = todoMgr->getCompletedTodos();

    for (const auto &todo : allTodos) {
        if (todo.dueDatetime >= weekStart && todo.dueDatetime <= weekEnd) {
            total++;
            if (todo.isCompleted) { completed++; completedItems << "- [x] " + todo.title; }
            else { pendingItems << "- [ ] " + todo.title; }
        }
    }
    for (const auto &todo : completedTodos) {
        if (todo.completedDatetime >= weekStart && todo.completedDatetime <= weekEnd) {
            bool found = false;
            for (const auto &t : allTodos) { if (t.id == todo.id) { found = true; break; } }
            if (!found) { total++; completed++; completedItems << "- [x] " + todo.title; }
        }
    }

    double rate = total > 0 ? (double)completed / total * 100 : 0;
    QString report;
    report += QString("# 周报：%1 - %2\n\n").arg(m_currentMonday.toString("yyyy-MM-dd"), sunday.toString("yyyy-MM-dd"));
    report += QString("## 本周总结\n\n共 %1 项待办，已完成 %2 项，完成率 %3%。\n\n").arg(total).arg(completed).arg(QString::number(rate, 'f', 1));
    report += "## 完成事项\n\n" + (completedItems.isEmpty() ? "无\n\n" : completedItems.join("\n") + "\n\n");
    report += "## 未完成事项\n\n" + (pendingItems.isEmpty() ? "无\n\n" : pendingItems.join("\n") + "\n\n");

    // 本周日程（7天分组，企业微信日程式）
    QStringList dayNames = {tr("周一"), tr("周二"), tr("周三"), tr("周四"), tr("周五"), tr("周六"), tr("周日")};
    report += "## 本周日程\n\n";
    for (int i = 0; i < 7; i++) {
        QDate d = m_currentMonday.addDays(i);
        qint64 dayStart = QDateTime(d, QTime(0, 0)).toSecsSinceEpoch();
        qint64 dayEnd = QDateTime(d, QTime(23, 59, 59)).toSecsSinceEpoch();

        QStringList dayLines;
        for (const auto &todo : allTodos) {
            qint64 match = todo.dueDatetime;
            if (match == 0) match = todo.creationDatetime;
            if (match >= dayStart && match <= dayEnd) {
                QString line = (todo.isCompleted ? "- [x] " : "- [ ] ") + todo.title;
                if (!todo.content.trimmed().isEmpty()) {
                    QString contentPreview = todo.content.trimmed();
                    if (contentPreview.length() > 40) contentPreview = contentPreview.left(40) + "...";
                    line += "（" + contentPreview + "）";
                }
                dayLines << line;
            }
        }
        report += QString("### %1 %2\n\n").arg(d.toString("MM-dd"), dayNames[i]);
        report += (dayLines.isEmpty() ? "无\n\n" : dayLines.join("\n") + "\n\n");
    }

    report += QString("笔记总数：%1\n\n").arg(app->noteManager()->noteCount());
    report += QString("---\n*自动生成于 %1*").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"));
    return report;
}

void WeeklyReportWidget::onGenerateReport()
{
    QString report = buildReportContent();
    auto *app = ShorthandApplication::instance();
    auto *ai = app->aiService();
    if (ai && ai->currentService()) {
        m_generateBtn->setEnabled(false);
        m_generateBtn->setText(tr("生成中..."));
        AiCompletionRequest req;
        AiMessage sysMsg;
        sysMsg.role = "system";
        sysMsg.content = tr("你是一个周报助手，根据数据生成专业周报，保持简洁明了。");
        req.messages.append(sysMsg);
        AiMessage userMsg;
        userMsg.role = "user";
        userMsg.content = report;
        req.messages.append(userMsg);
        req.temperature = 0.7;
        req.maxTokens = 2048;
        ai->complete(req, [this](const AiCompletionResult &result) {
            m_generateBtn->setEnabled(true);
            m_generateBtn->setText(tr("🤖 AI 生成周报"));
            m_reportPreview->setPlainText(result.success ? result.content : buildReportContent());
        });
    } else {
        m_reportPreview->setPlainText(report);
    }
}

void WeeklyReportWidget::onExportReport()
{
    QString content = m_reportPreview->toPlainText();
    if (content.trimmed().isEmpty()) {
        DDialog d(this);
        d.setTitle(tr("提示"));
        d.setMessage(tr("请先生成周报内容"));
        d.addButton(tr("确定"));
        d.exec();
        return;
    }
    QString fileName = QFileDialog::getSaveFileName(this, tr("导出周报"),
        QString("周报_%1.md").arg(m_currentMonday.toString("yyyyMMdd")),
        tr("Markdown (*.md);;文本文件 (*.txt)"));
    if (fileName.isEmpty()) return;
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << content;
        file.close();
        DDialog d(this);
        d.setTitle(tr("导出成功"));
        d.setMessage(tr("周报已导出到：%1").arg(fileName));
        d.addButton(tr("确定"));
        d.exec();
    }
}

#include "weeklyreportwidget.moc"
