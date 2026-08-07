#ifndef NOTEEDITORWIDGET_H
#define NOTEEDITORWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QColor>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTextImageFormat>
#include <QImage>
#include <QUrl>
#include <QVBoxLayout>
#include <QMenu>
#include <QTimer>
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

public slots:
    void onSave();

private slots:
    void onDelete();
    void onTagChanged();
    void togglePreview();
    void onAiAction(const QString &action);
    void onScreenshot();
    void onUndo();
    void onRedo();
    void updateWordCount();
    void onInsertImage();
    void onInsertScreenshot();
    bool onPasteImageFromClipboard();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void initUI();
    void initConnections();
    void setupToolbar(QVBoxLayout *mainLayout);
    void doAiComplete(const QString &systemPrompt, const QString &userText,
                      std::function<void(const QString &)> onResult);
    QString renderPreviewHtml() const;
    QString saveImageToAppData(const QString &srcPath) const;
    void insertImageMarkdown(const QString &path);

    int m_currentNoteId = -1;
    bool m_modified = false;
    bool m_previewMode = false;
    // 上次选择的文字颜色（TC03）：必须为成员变量，lambda 在栈变量析构后仍可能触发
    QColor m_lastTextColor;

    QLineEdit *m_titleEdit;
    QLabel *m_wordCountLabel;
    QToolButton *m_undoBtn;
    QToolButton *m_redoBtn;
    QComboBox *m_fontCombo;
    QComboBox *m_sizeCombo;
    QToolButton *m_boldBtn;
    QToolButton *m_italicBtn;
    QToolButton *m_underlineBtn;
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
    QTimer *m_autoSaveTimer;
    ScreenshotManager *m_screenshotMgr;
    AudioRecorder *m_recorder;
};

#endif // NOTEEDITORWIDGET_H