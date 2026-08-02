// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "weeklyreportwidget.h"
#include "application/shorthandapplication.h"
#include "core/todomanager.h"
#include "core/notemanager.h"
#include "services/aiservice.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <DLabel>
#include <DFontSizeManager>
#include <DDialog>
#include <DGuiApplicationHelper>
#include <QDebug>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QGridLayout>
#include <QDateEdit>
#include <QLineEdit>
#include <QMouseEvent>

// ─── 可双击的日历日期单元格 ───────────────────────────────────────
class ClickableDayCell : public QWidget
{
    Q_OBJECT
public:
    explicit ClickableDayCell(int dayIndex, QWidget *parent = nullptr)
        : QWidget(parent), m_dayIndex(dayIndex) {}

signals:
    void doubleClicked(int dayIndex);

protected:
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
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 12);
    mainLayout->setSpacing(8);

    // 头部导航
    QHBoxLayout *navRow = new QHBoxLayout();
    m_prevBtn = new QPushButton(tr("◀ 上一周"), this);
    m_prevBtn->setStyleSheet("QPushButton { background:transparent; border:1px solid palette(mid); border-radius:6px; padding:4px 12px; font-size:13px; color:palette(windowText); } QPushButton:hover { border-color:palette(highlight); color:palette(highlight); }");
    m_prevBtn->setFixedHeight(30);

    m_nextBtn = new QPushButton(tr("下一周 ▶"), this);
    m_nextBtn->setStyleSheet("QPushButton { background:transparent; border:1px solid palette(mid); border-radius:6px; padding:4px 12px; font-size:13px; color:palette(windowText); } QPushButton:hover { border-color:palette(highlight); color:palette(highlight); }");
    m_nextBtn->setFixedHeight(30);

    m_weekLabel = new DLabel(this);
    m_weekLabel->setStyleSheet("font-size: 14px; font-weight: 600;");
    m_weekLabel->setAlignment(Qt::AlignCenter);
    navRow->addWidget(m_prevBtn);
    navRow->addWidget(m_weekLabel, 1);
    navRow->addWidget(m_nextBtn);
    mainLayout->addLayout(navRow);

    // 横向日历网格（支持双击创建待办）
    QWidget *calendarWidget = new QWidget(this);
    calendarWidget->setStyleSheet("background: palette(light); border-radius: 6px;");
    QHBoxLayout *calendarLayout = new QHBoxLayout(calendarWidget);
    calendarLayout->setContentsMargins(8, 8, 8, 8);
    calendarLayout->setSpacing(4);

    QStringList dayNames = {tr("周一"), tr("周二"), tr("周三"), tr("周四"), tr("周五")};
    for (int i = 0; i < 5; i++) {
        ClickableDayCell *dayCell = new ClickableDayCell(i, this);
        QVBoxLayout *cellLayout = new QVBoxLayout(dayCell);
        cellLayout->setAlignment(Qt::AlignCenter);
        cellLayout->setSpacing(4);

        m_weekdayLabels[i] = new DLabel(dayNames[i], dayCell);
        m_weekdayLabels[i]->setAlignment(Qt::AlignCenter);
        m_weekdayLabels[i]->setStyleSheet("font-size: 11px; color: palette(placeholderText);");

        m_weekdayNums[i] = new DLabel(dayCell);
        m_weekdayNums[i]->setAlignment(Qt::AlignCenter);
        m_weekdayNums[i]->setFixedSize(32, 32);
        m_weekdayNums[i]->setStyleSheet("font-size: 14px; font-weight: 600; border-radius: 16px;");

        cellLayout->addWidget(m_weekdayLabels[i]);
        cellLayout->addWidget(m_weekdayNums[i]);

        // 双击日历日期 → 快速创建该日待办
        connect(dayCell, &ClickableDayCell::doubleClicked, this, [this](int dayIndex) {
            onDayCellDoubleClicked(dayIndex);
        });

        m_dayCells[i] = dayCell;
        calendarLayout->addWidget(dayCell, 1);
    }
    mainLayout->addWidget(calendarWidget);

    // 提示标签：双击日期可创建待办
    DLabel *hintLabel = new DLabel(tr("💡 双击日历日期可快速创建该日待办"), this);
    hintLabel->setStyleSheet("font-size: 11px; color: palette(placeholderText); padding: 2px 0;");
    hintLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(hintLabel);

    // 统计区
    QWidget *statsWidget = new QWidget(this);
    statsWidget->setStyleSheet("background: palette(light); border-radius: 6px;");
    QVBoxLayout *statsLayout = new QVBoxLayout(statsWidget);
    statsLayout->setContentsMargins(12, 8, 12, 8);
    statsLayout->setSpacing(4);

    m_totalLabel = new DLabel(this);
    m_totalLabel->setStyleSheet("font-size: 12px; color: palette(windowText);");
    m_completedLabel = new DLabel(this);
    m_completedLabel->setStyleSheet("font-size: 12px; color: palette(highlight);");
    m_pendingLabel = new DLabel(this);
    m_pendingLabel->setStyleSheet("font-size: 12px; color: palette(windowText);");
    m_overdueLabel = new DLabel(this);
    m_overdueLabel->setStyleSheet("font-size: 12px; color: palette(highlight);");
    m_rateLabel = new DLabel(this);
    m_rateLabel->setStyleSheet("font-size: 16px; font-weight: 700; color: palette(highlight);");

    statsLayout->addWidget(m_totalLabel);
    statsLayout->addWidget(m_completedLabel);
    statsLayout->addWidget(m_pendingLabel);
    statsLayout->addWidget(m_overdueLabel);
    statsLayout->addWidget(m_rateLabel);
    mainLayout->addWidget(statsWidget);

    // 已完成事项
    QWidget *completedWidget = new QWidget(this);
    QVBoxLayout *completedLayout = new QVBoxLayout(completedWidget);
    completedLayout->setContentsMargins(0, 0, 0, 0);
    DLabel *completedTitle = new DLabel(tr("已完成事项"), this);
    completedTitle->setStyleSheet("font-size: 12px; font-weight:600; color:palette(placeholderText); padding:4px 0;");
    completedLayout->addWidget(completedTitle);
    m_completedList = new QListWidget(this);
    m_completedList->setMaximumHeight(100);
    m_completedList->setFrameShape(QFrame::NoFrame);
    m_completedList->setStyleSheet("QListWidget { background:transparent; border:none; } QListWidget::item { font-size:12px; color:palette(windowText); padding:4px 0; }");
    completedLayout->addWidget(m_completedList);
    mainLayout->addWidget(completedWidget);

    // 未完成事项
    QWidget *pendingWidget = new QWidget(this);
    QVBoxLayout *pendingLayout = new QVBoxLayout(pendingWidget);
    pendingLayout->setContentsMargins(0, 0, 0, 0);
    DLabel *pendingTitle = new DLabel(tr("未完成事项"), this);
    pendingTitle->setStyleSheet("font-size: 12px; font-weight:600; color:palette(placeholderText); padding:4px 0;");
    pendingLayout->addWidget(pendingTitle);
    m_pendingList = new QListWidget(this);
    m_pendingList->setMaximumHeight(100);
    m_pendingList->setFrameShape(QFrame::NoFrame);
    m_pendingList->setStyleSheet("QListWidget { background:transparent; border:none; } QListWidget::item { font-size:12px; color:palette(windowText); padding:4px 0; }");
    pendingLayout->addWidget(m_pendingList);
    mainLayout->addWidget(pendingWidget);

    // 生成周报按钮
    QHBoxLayout *btnRow = new QHBoxLayout();
    m_generateBtn = new QPushButton(tr("🤖 AI 生成周报"), this);
    m_generateBtn->setStyleSheet("QPushButton { background:palette(highlight); color:palette(highlightedText); border:none; border-radius:6px; padding:6px 20px; font-size:13px; font-weight:600; } QPushButton:hover { background:palette(dark); } QPushButton:disabled { background:palette(mid); color:palette(windowText); }");
    m_generateBtn->setFixedHeight(34);

    m_exportBtn = new QPushButton(tr("📤 导出"), this);
    m_exportBtn->setStyleSheet("QPushButton { background:transparent; border:1px solid palette(mid); border-radius:6px; padding:6px 16px; font-size:12px; color:palette(windowText); } QPushButton:hover { border-color:palette(highlight); color:palette(highlight); }");
    m_exportBtn->setFixedHeight(34);

    btnRow->addWidget(m_generateBtn);
    btnRow->addWidget(m_exportBtn);
    btnRow->addStretch();
    mainLayout->addLayout(btnRow);

    // 周报预览
    DLabel *previewTitle = new DLabel(tr("周报预览"), this);
    previewTitle->setStyleSheet("font-size: 12px; font-weight:600; color:palette(placeholderText); padding:4px 0;");
    mainLayout->addWidget(previewTitle);

    m_reportPreview = new QTextEdit(this);
    m_reportPreview->setReadOnly(true);
    m_reportPreview->setPlaceholderText(tr("点击「AI 生成周报」自动生成\n或点击「📤 导出」导出为 Markdown"));
    mainLayout->addWidget(m_reportPreview, 1);

    connect(m_prevBtn, &QPushButton::clicked, this, &WeeklyReportWidget::onPrevWeek);
    connect(m_nextBtn, &QPushButton::clicked, this, &WeeklyReportWidget::onNextWeek);
    connect(m_generateBtn, &QPushButton::clicked, this, &WeeklyReportWidget::onGenerateReport);
    connect(m_exportBtn, &QPushButton::clicked, this, &WeeklyReportWidget::onExportReport);
}

void WeeklyReportWidget::onDayCellDoubleClicked(int dayIndex)
{
    if (dayIndex < 0 || dayIndex >= 5) return;

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
    for (int i = 0; i < 5; i++) {
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
    for (int i = 0; i < 5; i++) {
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
