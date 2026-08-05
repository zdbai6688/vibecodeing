// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WEEKLYREPORTWIDGET_H
#define WEEKLYREPORTWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QDate>
#include <DLabel>

DWIDGET_USE_NAMESPACE

class WeeklyReportWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(WeeklyReportWidget)

public:
    explicit WeeklyReportWidget(QWidget *parent = nullptr);
    void refresh();

private slots:
    void onPrevWeek();
    void onNextWeek();
    void onGenerateReport();
    void onExportReport();
    void onDayClicked(int dayIndex);

private:
    void initUI();
    QString buildReportContent();
    QString formatWeekDate(const QDate &date);
    void onDayCellDoubleClicked(int dayIndex);
    void updateCalendarCells();
    void applyDaySelection(int dayIndex); // -1 表示清除选中高亮
    void updateDayTodoList(const QDate &date);

    QDate m_currentMonday;
    QDate m_selectedDate;
    DLabel *m_weekLabel;
    DLabel *m_weekdayLabels[7];
    DLabel *m_weekdayNums[7];
    DLabel *m_dayTodoLabels[7];
    QWidget *m_dayCells[7]; // 五天的可点击区域，用于双击创建待办
    DLabel *m_totalLabel;
    DLabel *m_completedLabel;
    DLabel *m_pendingLabel;
    DLabel *m_overdueLabel;
    DLabel *m_rateLabel;
    DLabel *m_tagStatsLabel;
    QListWidget *m_completedList;
    QListWidget *m_pendingList;
    QLabel *m_dayTodoTitle;
    QListWidget *m_dayTodoList;
    QTextEdit *m_reportPreview;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_generateBtn;
    QPushButton *m_exportBtn;
};

#endif // WEEKLYREPORTWIDGET_H
