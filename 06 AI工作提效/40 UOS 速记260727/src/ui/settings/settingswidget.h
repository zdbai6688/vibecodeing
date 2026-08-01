#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <DSwitchButton>
#include <DPushButton>

DWIDGET_USE_NAMESPACE

class SettingsWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(SettingsWidget)

public:
    explicit SettingsWidget(QWidget *parent = nullptr);

    void loadSettings();
    void saveSettings();

signals:
    void settingsChanged();

private:
    void initUI();
    void initShortcutSection(QVBoxLayout *parent);
    void initAiSection(QVBoxLayout *parent);
    void initAsrSection(QVBoxLayout *parent);

    DSwitchButton *m_autostartSwitch;
    DSwitchButton *m_compactStartSwitch;
    QCheckBox *m_trayNotifyCheck;
    QSpinBox *m_reminderInterval;
    QLineEdit *m_shortcutEdit;

    QComboBox *m_themeCombo;
    QComboBox *m_aiEngineCombo;
    QLineEdit *m_deepseekKeyEdit;
    QLineEdit *m_tongyiKeyEdit;
    DPushButton *m_testBtn;

    QComboBox *m_asrEngineCombo;
    QLineEdit *m_baiduAsrKey;
    QLineEdit *m_baiduAsrSecret;
    QLineEdit *m_xunfeiAsrAppId;
    QLineEdit *m_xunfeiAsrKey;
    QLineEdit *m_xunfeiAsrSecret;
    QLineEdit *m_aliyunAsrKey;
    QLineEdit *m_aliyunAsrSecret;
    DPushButton *m_asrTestBtn;
};

#endif // SETTINGSWIDGET_H
