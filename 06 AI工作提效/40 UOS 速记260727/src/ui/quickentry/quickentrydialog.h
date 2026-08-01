#ifndef QUICKENTRYDIALOG_H
#define QUICKENTRYDIALOG_H

#include <QWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QStackedWidget>

class QuickEntryDialog : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(QuickEntryDialog)

public:
    explicit QuickEntryDialog(QWidget *parent = nullptr);

    void setFocus();
    void toggleCompactMode();
    bool isCompact() const { return m_compactMode; }
    bool isVisible() const { return !m_hidden; }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onSave();
    void onDiscard();
    void onVoiceInput();
    void onScreenshot();

private:
    void initUI();
    void initCompactUI();
    void initFullUI();
    void initConnections();
    void showCompact();
    void showFull();
    QString parseTags(const QString &text, QStringList &outTags);
    int parsePriority(const QString &text, QString &outText);
    void updateHint();
    void centerOnScreen();
    void applyCompactLayout();
    void applyFullLayout();

    QStackedWidget *m_modeStack;
    QWidget *m_compactPage;
    QWidget *m_fullPage;

    QTextEdit *m_contentEdit;
    QTextEdit *m_compactEdit;
    QLabel *m_hintLabel;
    QPushButton *m_voiceBtn;
    QPushButton *m_screenshotBtn;
    QPushButton *m_saveBtn;
    QPushButton *m_cancelBtn;
    QPushButton *m_expandBtn;
    QPushButton *m_compactBtn;
    QFrame *m_bottomBar;
    bool m_compactMode = true;
    bool m_hidden = true;
};

#endif // QUICKENTRYDIALOG_H
