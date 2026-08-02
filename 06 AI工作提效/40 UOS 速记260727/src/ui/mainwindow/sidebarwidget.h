#ifndef SIDEBARWIDGET_H
#define SIDEBARWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>

class SidebarWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(SidebarWidget)

public:
    explicit SidebarWidget(QWidget *parent = nullptr);

    void setActiveSection(int index);
    bool isTodoActive() const { return m_btnTodos && m_btnTodos->isChecked(); }
    void updateBadge(int notes, int todos);

signals:
    void notesClicked();
    void todosClicked();
    void meetingsClicked();
    void weeklyClicked();
    void trashClicked();
    void completedTodosClicked();
    void settingsClicked();
    void tagClicked(const QString &tag);
    void newNoteClicked();

private slots:
    void showTagContextMenu(const QPoint &pos);
    void onCreateTag();
    void onRenameTag();
    void onDeleteTag();

private:
    void initUI();
    void updateTagList();
    void updateStyleSheet();
    void addSectionLabel(const QString &text);
    QPushButton *makeNavBtn(const QString &icon, const QString &text, QLabel *&badgeLabel);

    QPushButton *m_btnNotes;
    QPushButton *m_btnTodos;
    QPushButton *m_btnMeetings;
    QPushButton *m_btnWeekly;
    QPushButton *m_btnTrash;
    QPushButton *m_btnCompleted;
    QPushButton *m_btnSettings;
    QPushButton *m_btnNewNote;
    QListWidget *m_tagList;
    QLabel *m_badgeNotes;
    QLabel *m_badgeTodos;
    QVBoxLayout *m_layout;
};

#endif // SIDEBARWIDGET_H
