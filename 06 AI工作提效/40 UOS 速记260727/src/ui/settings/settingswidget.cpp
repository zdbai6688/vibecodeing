// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingswidget.h"
#include "application/shorthandapplication.h"
#include "services/aiservice.h"
#include "globaldef.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <DLabel>
#include <DFontSizeManager>
#include <DGroupBox>
#include <DDialog>
#include <DGuiApplicationHelper>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

#include <QFile>
#include <QTextStream>
#include <QSettings>

static QString autostartDesktopPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart/uos-shorthand.desktop";
}

static bool isAutostartEnabled()
{
    return QFile::exists(autostartDesktopPath());
}

static void setAutostartEnabled(bool enable)
{
    QString path = autostartDesktopPath();
    if (enable) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=UOS速记\n";
            out << "Exec=uos-shorthand\n";
            out << "X-GNOME-Autostart-enabled=true\n";
            out << "NoDisplay=true\n";
            file.close();
        }
    } else {
        QFile::remove(path);
    }
}

SettingsWidget::SettingsWidget(QWidget *parent)
    : QWidget(parent)
{
    initUI();
    loadSettings();
}

void SettingsWidget::initUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 8, 16, 8);
    layout->setSpacing(12);

    DLabel *titleLabel = new DLabel(tr("⚙ 设置"), this);
    DFontSizeManager::instance()->bind(titleLabel, DFontSizeManager::T4, QFont::DemiBold);
    layout->addWidget(titleLabel);

    DGroupBox *generalGroup = new DGroupBox(tr("通用"), this);
    QVBoxLayout *generalLayout = new QVBoxLayout(generalGroup);
    generalLayout->setSpacing(12);

    QHBoxLayout *autostartRow = new QHBoxLayout();
    autostartRow->addWidget(new DLabel(tr("开机自启"), this));
    autostartRow->addStretch();
    m_autostartSwitch = new DSwitchButton(this);
    autostartRow->addWidget(m_autostartSwitch);
    generalLayout->addLayout(autostartRow);

    // 主题切换 (P2 #6)
    QHBoxLayout *themeRow = new QHBoxLayout();
    themeRow->addWidget(new DLabel(tr("主题"), this));
    themeRow->addStretch();
    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem(tr("跟随系统"));
    m_themeCombo->addItem(tr("浅色"));
    m_themeCombo->addItem(tr("深色"));
    m_themeCombo->setFixedWidth(200);
    themeRow->addWidget(m_themeCombo);
    generalLayout->addLayout(themeRow);

    QHBoxLayout *notifyRow = new QHBoxLayout();
    m_trayNotifyCheck = new QCheckBox(tr("启用系统托盘通知"), this);
    m_trayNotifyCheck->setChecked(true);
    notifyRow->addWidget(m_trayNotifyCheck);
    generalLayout->addLayout(notifyRow);

    layout->addWidget(generalGroup);

    DGroupBox *reminderGroup = new DGroupBox(tr("待办提醒"), this);
    QVBoxLayout *reminderLayout = new QVBoxLayout(reminderGroup);
    reminderLayout->setSpacing(12);

    QHBoxLayout *intervalRow = new QHBoxLayout();
    intervalRow->addWidget(new DLabel(tr("检查间隔（分钟）"), this));
    intervalRow->addStretch();
    m_reminderInterval = new QSpinBox(this);
    m_reminderInterval->setRange(1, 60);
    m_reminderInterval->setValue(5);
    m_reminderInterval->setSuffix(tr(" 分钟"));
    intervalRow->addWidget(m_reminderInterval);
    reminderLayout->addLayout(intervalRow);

    layout->addWidget(reminderGroup);

    initShortcutSection(layout);
    initAiSection(layout);
    initAsrSection(layout);

    layout->addStretch();

    connect(m_autostartSwitch, &DSwitchButton::toggled, this, [this](bool checked) {
        setAutostartEnabled(checked);
        emit settingsChanged();
    });
}

void SettingsWidget::initShortcutSection(QVBoxLayout *parent)
{
    DGroupBox *shortcutGroup = new DGroupBox(tr("⚡ 快捷键"), this);
    QVBoxLayout *shortcutLayout = new QVBoxLayout(shortcutGroup);
    shortcutLayout->setSpacing(10);

    QHBoxLayout *entryRow = new QHBoxLayout();
    entryRow->addWidget(new DLabel(tr("快速录入快捷键"), this));
    entryRow->addStretch();
    m_shortcutEdit = new QLineEdit(this);
    m_shortcutEdit->setText(SHORTCUT_QUICK_ENTRY);
    m_shortcutEdit->setFixedWidth(180);
    m_shortcutEdit->setAlignment(Qt::AlignCenter);
    m_shortcutEdit->setStyleSheet("QLineEdit { border:1px solid palette(mid); border-radius:4px; padding:4px 8px; font-size:12px; }");
    entryRow->addWidget(m_shortcutEdit);
    shortcutLayout->addLayout(entryRow);

    QLabel *shortcutTip = new QLabel(tr("💡 点击输入框后按下新的快捷键组合（如 Ctrl+Shift+N）"), this);
    shortcutTip->setStyleSheet("color:palette(placeholderText); font-size:11px;");
    shortcutLayout->addWidget(shortcutTip);

    QHBoxLayout *compactRow = new QHBoxLayout();
    compactRow->addWidget(new DLabel(tr("启动时显示紧凑窗口"), this));
    compactRow->addStretch();
    m_compactStartSwitch = new DSwitchButton(this);
    compactRow->addWidget(m_compactStartSwitch);
    shortcutLayout->addLayout(compactRow);

    QLabel *compactTip = new QLabel(tr("💡 启用后启动时自动弹出紧凑录入窗口"), this);
    compactTip->setStyleSheet("color:palette(placeholderText); font-size:11px;");
    shortcutLayout->addWidget(compactTip);

    parent->addWidget(shortcutGroup);

    connect(m_shortcutEdit, &QLineEdit::textChanged, this, [](const QString &v) { QSettings().setValue("shortcut/quick_entry", v); });
    connect(m_compactStartSwitch, &DSwitchButton::toggled, this, [](bool v) { QSettings().setValue("startup/compact_mode", v); });
}

void SettingsWidget::initAiSection(QVBoxLayout *parent)
{
    DGroupBox *aiGroup = new DGroupBox(tr("🤖 AI 服务"), this);
    QVBoxLayout *aiLayout = new QVBoxLayout(aiGroup);
    aiLayout->setSpacing(10);

    QHBoxLayout *engineRow = new QHBoxLayout();
    engineRow->addWidget(new DLabel(tr("AI 引擎"), this));
    engineRow->addStretch();
    m_aiEngineCombo = new QComboBox(this);
    m_aiEngineCombo->addItem("DeepSeek");
    m_aiEngineCombo->addItem(tr("通义千问"));
    m_aiEngineCombo->setFixedWidth(200);
    engineRow->addWidget(m_aiEngineCombo);
    aiLayout->addLayout(engineRow);

    auto addKeyRow = [this, aiLayout](const QString &label, const QString &placeholder, QLineEdit *&edit) {
        QHBoxLayout *row = new QHBoxLayout();
        row->addWidget(new DLabel(label, this));
        row->addStretch();
        edit = new QLineEdit(this);
        edit->setPlaceholderText(placeholder);
        edit->setEchoMode(QLineEdit::Password);
        edit->setFixedWidth(280);
        row->addWidget(edit);
        aiLayout->addLayout(row);
    };

    addKeyRow(tr("DeepSeek API Key"), "sk-...", m_deepseekKeyEdit);
    addKeyRow(tr("通义千问 API Key"), "sk-...", m_tongyiKeyEdit);

    QHBoxLayout *testRow = new QHBoxLayout();
    testRow->addStretch();
    m_testBtn = new DPushButton(tr("测试连接"), this);
    testRow->addWidget(m_testBtn);
    aiLayout->addLayout(testRow);

    parent->addWidget(aiGroup);

    connect(m_aiEngineCombo, &QComboBox::currentTextChanged, this, [](const QString &engine) {
        QSettings().setValue("ai/engine", engine);
    });
    connect(m_deepseekKeyEdit, &QLineEdit::textChanged, this, [](const QString &v) { QSettings().setValue("ai/deepseek_key", v); });
    connect(m_tongyiKeyEdit, &QLineEdit::textChanged, this, [](const QString &v) { QSettings().setValue("ai/tongyi_key", v); });
    connect(m_testBtn, &DPushButton::clicked, this, [this]() {
        DDialog d(this);
        d.setTitle(tr("测试连接"));
        d.setMessage(tr("AI 连接测试功能将在后续版本中实现。\n请先在对应平台注册并获取 API Key。"));
        d.addButton(tr("确定"));
        d.exec();
    });
}

void SettingsWidget::initAsrSection(QVBoxLayout *parent)
{
    DGroupBox *asrGroup = new DGroupBox(tr("🎤 语音识别"), this);
    QVBoxLayout *asrLayout = new QVBoxLayout(asrGroup);
    asrLayout->setSpacing(10);

    QHBoxLayout *engineRow = new QHBoxLayout();
    engineRow->addWidget(new DLabel(tr("ASR 引擎"), this));
    engineRow->addStretch();
    m_asrEngineCombo = new QComboBox(this);
    m_asrEngineCombo->addItem(tr("百度语音"));
    m_asrEngineCombo->addItem(tr("讯飞语音"));
    m_asrEngineCombo->addItem(tr("阿里云语音"));
    m_asrEngineCombo->setFixedWidth(200);
    engineRow->addWidget(m_asrEngineCombo);
    asrLayout->addLayout(engineRow);

    auto addKeyRow = [this, asrLayout](const QString &label, const QString &placeholder, QLineEdit *&edit) {
        QHBoxLayout *row = new QHBoxLayout();
        row->addWidget(new DLabel(label, this));
        row->addStretch();
        edit = new QLineEdit(this);
        edit->setPlaceholderText(placeholder);
        edit->setEchoMode(QLineEdit::Password);
        edit->setFixedWidth(280);
        row->addWidget(edit);
        asrLayout->addLayout(row);
    };

    DLabel *baiduTitle = new DLabel(tr("百度语音（推荐）"), this);
    baiduTitle->setStyleSheet("font-weight:600; padding:4px 0; font-size:12px;");
    asrLayout->addWidget(baiduTitle);
    addKeyRow(tr("API Key"), "4n2E...", m_baiduAsrKey);
    addKeyRow(tr("Secret Key"), "e9c3...", m_baiduAsrSecret);

    DLabel *xunfeiTitle = new DLabel(tr("讯飞语音"), this);
    xunfeiTitle->setStyleSheet("font-weight:600; padding:4px 0; font-size:12px;");
    asrLayout->addWidget(xunfeiTitle);
    addKeyRow(tr("APP ID"), "5f3a...", m_xunfeiAsrAppId);
    addKeyRow(tr("API Key"), "a1b2...", m_xunfeiAsrKey);
    addKeyRow(tr("API Secret"), "MjM2...", m_xunfeiAsrSecret);

    DLabel *aliTitle = new DLabel(tr("阿里云语音"), this);
    aliTitle->setStyleSheet("font-weight:600; padding:4px 0; font-size:12px;");
    asrLayout->addWidget(aliTitle);
    addKeyRow(tr("Access Key ID"), "LTAI...", m_aliyunAsrKey);
    addKeyRow(tr("Access Key Secret"), "e2c9...", m_aliyunAsrSecret);

    QHBoxLayout *testRow = new QHBoxLayout();
    testRow->addStretch();
    m_asrTestBtn = new DPushButton(tr("测试连接"), this);
    testRow->addWidget(m_asrTestBtn);
    asrLayout->addLayout(testRow);

    QLabel *tip = new QLabel(tr("💡 注册地址：百度 ai.baidu.com | 讯飞 xfyun.cn | 阿里云 nls.aliyun.com"), this);
    tip->setStyleSheet("color:palette(placeholderText); font-size:11px; padding:4px 0;");
    asrLayout->addWidget(tip);

    parent->addWidget(asrGroup);

    connect(m_asrEngineCombo, &QComboBox::currentTextChanged, this, [](const QString &engine) {
        QSettings().setValue("asr/engine", engine);
    });
    connect(m_baiduAsrKey, &QLineEdit::textChanged, this, [](const QString &v) { QSettings().setValue("asr/baidu_key", v); });
    connect(m_baiduAsrSecret, &QLineEdit::textChanged, this, [](const QString &v) { QSettings().setValue("asr/baidu_secret", v); });
    connect(m_xunfeiAsrAppId, &QLineEdit::textChanged, this, [](const QString &v) { QSettings().setValue("asr/xunfei_appid", v); });
    connect(m_xunfeiAsrKey, &QLineEdit::textChanged, this, [](const QString &v) { QSettings().setValue("asr/xunfei_key", v); });
    connect(m_xunfeiAsrSecret, &QLineEdit::textChanged, this, [](const QString &v) { QSettings().setValue("asr/xunfei_secret", v); });
    connect(m_aliyunAsrKey, &QLineEdit::textChanged, this, [](const QString &v) { QSettings().setValue("asr/aliyun_key", v); });
    connect(m_aliyunAsrSecret, &QLineEdit::textChanged, this, [](const QString &v) { QSettings().setValue("asr/aliyun_secret", v); });
    connect(m_asrTestBtn, &DPushButton::clicked, this, [this]() {
        DDialog d(this);
        d.setTitle(tr("提示"));
        d.setMessage(tr("ASR 连接测试功能将在后续版本中实现。\n请先在对应平台注册并获取 API Key。"));
        d.addButton(tr("确定"));
        d.exec();
    });
}

void SettingsWidget::loadSettings()
{
    m_autostartSwitch->setChecked(isAutostartEnabled());

    QSettings settings;

    // 加载主题设置
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
}

void SettingsWidget::saveSettings()
{
    setAutostartEnabled(m_autostartSwitch->isChecked());

    QSettings settings;
    settings.setValue("appearance/theme", m_themeCombo->currentIndex());
}
