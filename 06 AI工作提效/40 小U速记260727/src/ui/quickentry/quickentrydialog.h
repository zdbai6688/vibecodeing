// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

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
#include <DSwitchButton>
#include <QMouseEvent>

class EdgeAutoHide;

DWIDGET_USE_NAMESPACE

class QuickEntryDialog : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(QuickEntryDialog)

public:
    explicit QuickEntryDialog(QWidget *parent = nullptr);

    void setFocus();
    bool isVisible() const { return !m_hidden; }
    void setPasteToDesktopMode(bool on);
    void setCascadeIndex(int index);

    // 关闭窗口并保留未保存内容为草稿（供下一个新窗口恢复）
    void dismissWithDraft();

signals:
    void pinToDesktopRequested(int noteId);
    // 窗口已关闭（保存/丢弃/关闭按钮），由 MainWindow 负责清理
    void dismissed();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onSave();
    void onDiscard();
    void onVoiceInput();
    void onScreenshot();
    void onPinToDesktop();

private:
    void initUI();
    void initConnections();
    void showCompact();
    void showFull();
    QString parseTags(const QString &text, QStringList &outTags);
    int parsePriority(const QString &text, QString &outText);
    void updateHint();
    void centerOnScreen();
    void enterGhostState();
    void leaveGhostState();
    QString draftPath() const;
    void restorePendingDraft();

    QWidget *m_page = nullptr;         // 统一单页（TC14 二轮：取消紧凑/全屏双尺寸切换）

    QTextEdit *m_contentEdit;
    QLabel *m_hintLabel;
    QPushButton *m_saveBtn;
    QPushButton *m_pinToDesktopBtn;
    QWidget *m_dragBar;
    EdgeAutoHide *m_edgeHide = nullptr;
    DSwitchButton *m_continuousSwitch;
    QFrame *m_bottomBar;
    bool m_hidden = true;
    bool m_ghostState = false;
    bool m_continuousAdd = false;
    bool m_waitingForPin = false;
    int m_lastSavedNoteId = -1;
    int m_instanceId = 0;
    int m_cascadeIndex = 0;
};

#endif // QUICKENTRYDIALOG_H
