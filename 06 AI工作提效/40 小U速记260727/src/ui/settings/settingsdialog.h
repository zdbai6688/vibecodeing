// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <DDialog>
#include <QString>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QTabWidget>
#include <QStackedWidget>
#include <QSlider>
#include <QPlainTextEdit>
#include <DSwitchButton>
#include <DPushButton>

DWIDGET_USE_NAMESPACE

class SettingsDialog : public DDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(SettingsDialog)

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    void loadSettings();
    void saveSettings();

protected:
    // 捕获快捷键输入：点击输入框后直接按下新的快捷键组合
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void initUI();
    QString normalizedShortcut(const QString &text) const;
    QWidget *createGeneralPage();
    QWidget *createAiPage();
    QWidget *createAsrPage();
    QWidget *createShortcutPage();
    QWidget *createDesktopPage();
    QWidget *createBackupPage();
    QWidget *createTagsPage();
    QWidget *createWeeklyTemplatePage();
    void refreshTagRows();

    DSwitchButton *m_autostartSwitch;
    QCheckBox *m_trayNotifyCheck;
    QSpinBox *m_reminderInterval;
    QLineEdit *m_recordingDirEdit;

    QComboBox *m_themeCombo;
    QComboBox *m_languageCombo;
    QComboBox *m_aiEngineCombo;
    QLineEdit *m_deepseekKeyEdit;
    QLineEdit *m_tongyiKeyEdit;
    DPushButton *m_aiTestBtn;

    QComboBox *m_asrEngineCombo;
    QLineEdit *m_baiduAsrKey;
    QLineEdit *m_baiduAsrSecret;
    QLineEdit *m_xunfeiAsrAppId;
    QLineEdit *m_xunfeiAsrKey;
    QLineEdit *m_xunfeiAsrSecret;
    QLineEdit *m_aliyunAsrKey;
    QLineEdit *m_aliyunAsrSecret;
    DPushButton *m_asrTestBtn;

    QLineEdit *m_shortcutEdit;
    QString m_lastShortcut;   // 最近一次有效的快捷键（用于失焦恢复）
    DSwitchButton *m_compactStartSwitch;

    // Desktop mode settings
    DSwitchButton *m_desktopStartSwitch;
    DSwitchButton *m_continuousAddSwitch;
    QComboBox *m_defaultColorCombo;
    QSlider *m_opacitySlider;
    QSpinBox *m_maxNotesSpin;

    // 备份/恢复
    QLabel *m_backupLastPathLabel;

    // 标签颜色
    QVBoxLayout *m_tagsLayout = nullptr;
    QWidget *m_tagsContainer = nullptr;
    QMap<QString, QColor> m_tagColorEdits;  // 标签名 -> 当前选中颜色

    // 周报模板
    QPlainTextEdit *m_weeklyTemplateEdit = nullptr;
};

#endif // SETTINGSDIALOG_H
