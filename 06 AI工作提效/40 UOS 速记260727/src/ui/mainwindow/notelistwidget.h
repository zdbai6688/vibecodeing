#ifndef NOTELISTWIDGET_H
#define NOTELISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStackedWidget>
#include <DSpinner>
#include "storage/notestorage.h"

DWIDGET_USE_NAMESPACE

class NoteListWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(NoteListWidget)

public:
    enum Mode { AllNotes, TagFilter, Trash, Search };

    explicit NoteListWidget(QWidget *parent = nullptr);

    void setMode(Mode mode);
    void setFilterTag(const QString &tag);
    void setSearchKeyword(const QString &keyword);
    void refresh();
    void selectNote(int noteId);

signals:
    void noteSelected(int noteId);

private:
    void initUI();
    void populateList(const QList<NoteData> &notes);
    void showLoading(bool loading);
    QString getEmptyIcon(Mode mode) const;
    QString getEmptyTitle(Mode mode) const;
    QString getEmptyHint(Mode mode) const;

    QListWidget *m_list;
    QStackedWidget *m_stack;
    DSpinner *m_spinner;
    Mode m_mode = AllNotes;
    QString m_filterTag;
    QString m_searchKeyword;
};

#endif // NOTELISTWIDGET_H
