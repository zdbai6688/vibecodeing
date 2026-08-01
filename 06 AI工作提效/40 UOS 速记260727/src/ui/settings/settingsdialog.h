#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <DDialog>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QTabWidget>
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

private:
    void initUI();
    QWidget *createGeneralPage();
    QWidget *createAiPage();
    QWidget *createAsrPage();
    QWidget *createShortcutPage();

    DSwitchButton *m_autostartSwitch;
    QCheckBox *m_trayNotifyCheck;
    QSpinBox *m_reminderInterval;

    QComboBox *m_themeCombo;
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
    DSwitchButton *m_compactStartSwitch;
};

#endif // SETTINGSDIALOG_H
