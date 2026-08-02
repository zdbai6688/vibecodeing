#ifndef NOTELISTWIDGET_H
#define NOTELISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStackedWidget>
#include <QComboBox>
#include <DToolButton>
#include <DSpinner>
#include <DLabel>
#include <QPushButton>
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

    // 批量操作
    void enterMultiSelectMode();
    void exitMultiSelectMode();
    void updateSelectionState();
    void onBatchDelete();
    void onBatchExport();
    QList<int> getSelectedNoteIds() const;

    QListWidget *m_list;
    QStackedWidget *m_stack;
    DSpinner *m_spinner;
    Mode m_mode = AllNotes;
    QString m_filterTag;
    QString m_searchKeyword;

    // 排序
    void loadSortPreference();
    void saveSortPreference();
    QComboBox *m_sortFieldCombo;
    DToolButton *m_sortOrderBtn;
    NoteSortParam m_sortParam;

    // 批量操作控件
    bool m_multiSelectMode = false;
    QWidget *m_batchToolbar;
    QPushButton *m_selectModeBtn;
    QPushButton *m_selectAllBtn;
    QPushButton *m_batchDeleteBtn;
    QPushButton *m_batchExportBtn;
    DLabel *m_selectionCountLabel;
};

#endif // NOTELISTWIDGET_H
