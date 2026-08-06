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
#include <QMap>
#include <DLabel>

DWIDGET_USE_NAMESPACE

class WeeklyReportWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(WeeklyReportWidget)

public:
    explicit WeeklyReportWidget(QWidget *parent = nullptr);
    void refresh();

    // 默认周报模板（设置页用于「恢复默认」展示）
    static const char *kDefaultTemplate;

    // 模板占位符替换（{key} → 区块），供单测与设置页预览复用
    static QString renderTemplate(const QString &tpl, const QMap<QString, QString> &sections)
    {
        // 未定义的占位符保持原样，避免误删用户模板内容
        QString out = tpl;
        QMapIterator<QString, QString> it(sections);
        while (it.hasNext()) {
            it.next();
            out.replace(QString("{%1}").arg(it.key()), it.value());
        }
        return out;
    }

private slots:
    void onPrevWeek();
    void onNextWeek();
    void onGenerateReport();
    void onExportReport();
    void onDayClicked(int dayIndex);

private:
    void initUI();
    QString buildReportContent();
    QMap<QString, QString> buildReportSections();
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
