// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"
#include "application/shorthandapplication.h"
#include "services/aiservice.h"
#include "globaldef.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <DLabel>
#include <DFontSizeManager>
#include <DDialog>
#include <DGuiApplicationHelper>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QStandardPaths>

SettingsDialog::SettingsDialog(QWidget *parent)
    : DDialog(parent)
{
    setWindowTitle(tr("设置"));
    setFixedSize(600, 520);

    QTabWidget *tabs = new QTabWidget(this);
    tabs->addTab(createGeneralPage(), tr("通用"));
    tabs->addTab(createAiPage(), tr("AI 服务"));
    tabs->addTab(createAsrPage(), tr("语音识别"));
    tabs->addTab(createShortcutPage(), tr("快捷键"));
    addContent(tabs);
}

QWidget *SettingsDialog::createGeneralPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    QHBoxLayout *autostartRow = new QHBoxLayout();
    autostartRow->addWidget(new DLabel(tr("开机自启"), this));
    autostartRow->addStretch();
    m_autostartSwitch = new DSwitchButton(this);
    autostartRow->addWidget(m_autostartSwitch);
    layout->addLayout(autostartRow);

    QHBoxLayout *themeRow = new QHBoxLayout();
    themeRow->addWidget(new DLabel(tr("主题"), this));
    themeRow->addStretch();
    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem(tr("跟随系统"));
    m_themeCombo->addItem(tr("浅色"));
    m_themeCombo->addItem(tr("深色"));
    m_themeCombo->setFixedWidth(200);
    themeRow->addWidget(m_themeCombo);
    layout->addLayout(themeRow);

    QHBoxLayout *notifyRow = new QHBoxLayout();
    m_trayNotifyCheck = new QCheckBox(tr("启用系统托盘通知"), this);
    notifyRow->addWidget(m_trayNotifyCheck);
    layout->addLayout(notifyRow);

    QHBoxLayout *intervalRow = new QHBoxLayout();
    intervalRow->addWidget(new DLabel(tr("待办提醒检查间隔"), this));
    intervalRow->addStretch();
    m_reminderInterval = new QSpinBox(this);
    m_reminderInterval->setRange(1, 60);
    m_reminderInterval->setValue(5);
    m_reminderInterval->setSuffix(tr(" 分钟"));
    intervalRow->addWidget(m_reminderInterval);
    layout->addLayout(intervalRow);

    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createAiPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    QHBoxLayout *engineRow = new QHBoxLayout();
    engineRow->addWidget(new DLabel(tr("AI 引擎"), this));
    engineRow->addStretch();
    m_aiEngineCombo = new QComboBox(this);
    m_aiEngineCombo->addItem("DeepSeek");
    m_aiEngineCombo->addItem(tr("通义千问"));
    m_aiEngineCombo->setFixedWidth(200);
    engineRow->addWidget(m_aiEngineCombo);
    layout->addLayout(engineRow);

    auto addRow = [this, layout](const QString &label, QLineEdit *&edit) {
        QHBoxLayout *row = new QHBoxLayout();
        row->addWidget(new DLabel(label, this));
        row->addStretch();
        edit = new QLineEdit(this);
        edit->setEchoMode(QLineEdit::Password);
        edit->setFixedWidth(300);
        row->addWidget(edit);
        layout->addLayout(row);
    };

    addRow(tr("DeepSeek API Key"), m_deepseekKeyEdit);
    addRow(tr("通义千问 API Key"), m_tongyiKeyEdit);

    QHBoxLayout *testRow = new QHBoxLayout();
    testRow->addStretch();
    m_aiTestBtn = new DPushButton(tr("测试连接"), this);
    testRow->addWidget(m_aiTestBtn);
    layout->addLayout(testRow);

    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createAsrPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);

    QHBoxLayout *engineRow = new QHBoxLayout();
    engineRow->addWidget(new DLabel(tr("ASR 引擎"), this));
    engineRow->addStretch();
    m_asrEngineCombo = new QComboBox(this);
    m_asrEngineCombo->addItem(tr("离线语音 (Whisper)"));
    m_asrEngineCombo->addItem(tr("百度语音"));
    m_asrEngineCombo->addItem(tr("讯飞语音"));
    m_asrEngineCombo->addItem(tr("阿里云语音"));
    m_asrEngineCombo->setFixedWidth(200);
    engineRow->addWidget(m_asrEngineCombo);
    layout->addLayout(engineRow);

    auto addRow = [this, layout](const QString &label, QLineEdit *&edit) {
        QHBoxLayout *row = new QHBoxLayout();
        row->addWidget(new DLabel(label, this));
        row->addStretch();
        edit = new QLineEdit(this);
        edit->setEchoMode(QLineEdit::Password);
        edit->setFixedWidth(280);
        row->addWidget(edit);
        layout->addLayout(row);
    };

    DLabel *baiduTitle = new DLabel(tr("百度语音（推荐）"), this);
    baiduTitle->setStyleSheet("font-weight:600; font-size:12px;");
    layout->addWidget(baiduTitle);
    addRow(tr("API Key"), m_baiduAsrKey);
    addRow(tr("Secret Key"), m_baiduAsrSecret);

    DLabel *xunfeiTitle = new DLabel(tr("讯飞语音"), this);
    xunfeiTitle->setStyleSheet("font-weight:600; font-size:12px;");
    layout->addWidget(xunfeiTitle);
    addRow(tr("APP ID"), m_xunfeiAsrAppId);
    addRow(tr("API Key"), m_xunfeiAsrKey);
    addRow(tr("API Secret"), m_xunfeiAsrSecret);

    DLabel *aliTitle = new DLabel(tr("阿里云语音"), this);
    aliTitle->setStyleSheet("font-weight:600; font-size:12px;");
    layout->addWidget(aliTitle);
    addRow(tr("Access Key ID"), m_aliyunAsrKey);
    addRow(tr("Access Key Secret"), m_aliyunAsrSecret);

    QHBoxLayout *testRow = new QHBoxLayout();
    testRow->addStretch();
    m_asrTestBtn = new DPushButton(tr("测试连接"), this);
    testRow->addWidget(m_asrTestBtn);
    layout->addLayout(testRow);

    QLabel *tip = new QLabel(tr("注册地址：百度 ai.baidu.com | 讯飞 xfyun.cn | 阿里云 nls.aliyun.com"), this);
    tip->setStyleSheet("color:palette(placeholderText); font-size:11px;");
    layout->addWidget(tip);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createShortcutPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    QHBoxLayout *entryRow = new QHBoxLayout();
    entryRow->addWidget(new DLabel(tr("快速录入快捷键"), this));
    entryRow->addStretch();
    m_shortcutEdit = new QLineEdit(this);
    m_shortcutEdit->setText(SHORTCUT_QUICK_ENTRY);
    m_shortcutEdit->setFixedWidth(180);
    m_shortcutEdit->setAlignment(Qt::AlignCenter);
    entryRow->addWidget(m_shortcutEdit);
    layout->addLayout(entryRow);

    QLabel *tip = new QLabel(tr("点击输入框后按下新的快捷键组合"), this);
    tip->setStyleSheet("color:palette(placeholderText); font-size:11px;");
    layout->addWidget(tip);

    QHBoxLayout *compactRow = new QHBoxLayout();
    compactRow->addWidget(new DLabel(tr("启动时显示紧凑窗口"), this));
    compactRow->addStretch();
    m_compactStartSwitch = new DSwitchButton(this);
    compactRow->addWidget(m_compactStartSwitch);
    layout->addLayout(compactRow);

    layout->addStretch();
    return page;
}

void SettingsDialog::loadSettings()
{
    QSettings settings;

    m_autostartSwitch->setChecked(QFile::exists(
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart/uos-shorthand.desktop"));

    int themeType = settings.value("appearance/theme", 0).toInt();
    m_themeCombo->setCurrentIndex(themeType);

    QString engine = settings.value("ai/engine", "DeepSeek").toString();
    int idx = m_aiEngineCombo->findText(engine);
    if (idx >= 0) m_aiEngineCombo->setCurrentIndex(idx);
    m_deepseekKeyEdit->setText(settings.value("ai/deepseek_key").toString());
    m_tongyiKeyEdit->setText(settings.value("ai/tongyi_key").toString());

    QString asrEngine = settings.value("asr/engine", "百度语音").toString();
    int asrIdx = m_asrEngineCombo->findText(asrEngine);
    if (asrIdx >= 0) m_asrEngineCombo->setCurrentIndex(asrIdx);
    m_baiduAsrKey->setText(settings.value("asr/baidu_key").toString());
    m_baiduAsrSecret->setText(settings.value("asr/baidu_secret").toString());
    m_xunfeiAsrAppId->setText(settings.value("asr/xunfei_appid").toString());
    m_xunfeiAsrKey->setText(settings.value("asr/xunfei_key").toString());
    m_xunfeiAsrSecret->setText(settings.value("asr/xunfei_secret").toString());
    m_aliyunAsrKey->setText(settings.value("asr/aliyun_key").toString());
    m_aliyunAsrSecret->setText(settings.value("asr/aliyun_secret").toString());

    m_shortcutEdit->setText(settings.value("shortcut/quick_entry", SHORTCUT_QUICK_ENTRY).toString());
    m_compactStartSwitch->setChecked(settings.value("startup/compact_mode", false).toBool());
}

void SettingsDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("appearance/theme", m_themeCombo->currentIndex());
    settings.setValue("ai/engine", m_aiEngineCombo->currentText());
    settings.setValue("ai/deepseek_key", m_deepseekKeyEdit->text());
    settings.setValue("ai/tongyi_key", m_tongyiKeyEdit->text());
    settings.setValue("asr/engine", m_asrEngineCombo->currentText());
    settings.setValue("asr/baidu_key", m_baiduAsrKey->text());
    settings.setValue("asr/baidu_secret", m_baiduAsrSecret->text());
    settings.setValue("asr/xunfei_appid", m_xunfeiAsrAppId->text());
    settings.setValue("asr/xunfei_key", m_xunfeiAsrKey->text());
    settings.setValue("asr/xunfei_secret", m_xunfeiAsrSecret->text());
    settings.setValue("asr/aliyun_key", m_aliyunAsrKey->text());
    settings.setValue("asr/aliyun_secret", m_aliyunAsrSecret->text());
    settings.setValue("shortcut/quick_entry", m_shortcutEdit->text());
    settings.setValue("startup/compact_mode", m_compactStartSwitch->isChecked());
}
