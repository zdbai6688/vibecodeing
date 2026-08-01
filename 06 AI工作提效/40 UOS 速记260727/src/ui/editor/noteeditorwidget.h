#ifndef NOTEEDITORWIDGET_H
#define NOTEEDITORWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QMenu>
#include "storage/notestorage.h"
#include "services/aiservice.h"
#include "services/exportservice.h"
#include "services/asrservice.h"
#include "services/screenshotmanager.h"
#include "audio/audiorecorder.h"

class MarkdownHighlighter;

class NoteEditorWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(NoteEditorWidget)

public:
    explicit NoteEditorWidget(QWidget *parent = nullptr);
    void loadNote(int noteId);
    void clearEditor();

private slots:
    void onSave();
    void onDelete();
    void onTagChanged();
    void togglePreview();
    void onAiAction(const QString &action);
    void onScreenshot();
    void onUndo();
    void onRedo();
    void onInsertImage();

private:
    void initUI();
    void initConnections();
    void setupToolbar(QVBoxLayout *mainLayout);
    void doAiComplete(const QString &systemPrompt, const QString &userText,
                      std::function<void(const QString &)> onResult);

    int m_currentNoteId = -1;
    bool m_modified = false;
    bool m_previewMode = false;

    QLineEdit *m_titleEdit;
    QToolButton *m_undoBtn;
    QToolButton *m_redoBtn;
    QComboBox *m_fontCombo;
    QComboBox *m_sizeCombo;
    QToolButton *m_boldBtn;
    QToolButton *m_olBtn;
    QToolButton *m_ulBtn;
    QToolButton *m_imageBtn;
    QStackedWidget *m_contentStack;
    QTextEdit *m_contentEdit;
    QTextBrowser *m_previewBrowser;
    MarkdownHighlighter *m_highlighter;
    QComboBox *m_tagCombo;
    QPushButton *m_saveBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_todoToggleBtn;
    QPushButton *m_previewBtn;
    QPushButton *m_voiceBtn;
    ScreenshotManager *m_screenshotMgr;
    AudioRecorder *m_recorder;
};

#endif // NOTEEDITORWIDGET_H