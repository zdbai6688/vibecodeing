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

private:
    void initUI();
    QString buildReportContent();
    QString formatWeekDate(const QDate &date);

    QDate m_currentMonday;
    DLabel *m_weekLabel;
    DLabel *m_weekdayLabels[5];
    DLabel *m_weekdayNums[5];
    DLabel *m_totalLabel;
    DLabel *m_completedLabel;
    DLabel *m_pendingLabel;
    DLabel *m_overdueLabel;
    DLabel *m_rateLabel;
    QListWidget *m_completedList;
    QListWidget *m_pendingList;
    QTextEdit *m_reportPreview;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_generateBtn;
    QPushButton *m_exportBtn;
};

#endif // WEEKLYREPORTWIDGET_H
