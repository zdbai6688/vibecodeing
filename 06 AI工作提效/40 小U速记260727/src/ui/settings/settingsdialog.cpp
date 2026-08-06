// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"
#include "application/shorthandapplication.h"
#include "services/aiservice.h"
#include "services/asrservice.h"
#include "services/cryptoutil.h"
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
#include <QFileInfo>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <DGroupBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QKeyEvent>
#include <QEvent>

SettingsDialog::SettingsDialog(QWidget *parent)
    : DDialog(parent)
{
    setWindowTitle(tr("设置"));
    setFixedSize(600, 560);

    QTabWidget *tabs = new QTabWidget(this);
    tabs->addTab(createGeneralPage(), tr("通用"));
    tabs->addTab(createDesktopPage(), tr("桌面模式"));
    tabs->addTab(createAiPage(), tr("AI 服务"));
    tabs->addTab(createAsrPage(), tr("语音识别"));
    tabs->addTab(createShortcutPage(), tr("快捷键"));
    addContent(tabs);

    // 保存统一由 MainWindow::onShowSettings 在 exec 返回 Accepted 后调用，
    // 避免此处自动保存导致“取消”也写入配置。
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

    // 语言切换（IDE-201 P4-T9）：保存语言代码，重启后生效
    QHBoxLayout *languageRow = new QHBoxLayout();
    languageRow->addWidget(new DLabel(tr("语言"), this));
    languageRow->addStretch();
    m_languageCombo = new QComboBox(this);
    m_languageCombo->addItem(tr("中文"), QStringLiteral("zh_CN"));
    m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en_US"));
    m_languageCombo->setFixedWidth(200);
    languageRow->addWidget(m_languageCombo);
    layout->addLayout(languageRow);

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

    // 录音存储目录
    QHBoxLayout *recDirRow = new QHBoxLayout();
    recDirRow->addWidget(new DLabel(tr("录音存储目录"), this));
    recDirRow->addStretch();
    m_recordingDirEdit = new QLineEdit(this);
    m_recordingDirEdit->setPlaceholderText(tr("默认：文档/UOS速记/录音"));
    m_recordingDirEdit->setFixedWidth(280);
    recDirRow->addWidget(m_recordingDirEdit);
    QPushButton *recDirBtn = new QPushButton(tr("浏览…"), this);
    recDirBtn->setFixedWidth(64);
    recDirBtn->setCursor(Qt::PointingHandCursor);
    recDirRow->addWidget(recDirBtn);
    layout->addLayout(recDirRow);
    connect(recDirBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("选择录音存储目录"),
                                                        m_recordingDirEdit->text().trimmed());
        if (!dir.isEmpty()) {
            m_recordingDirEdit->setText(dir);
        }
    });

    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createDesktopPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    DLabel *titleLabel = new DLabel(tr("🖥 桌面模式设置"), this);
    titleLabel->setStyleSheet("font-size: 15px; font-weight: 600;");
    layout->addWidget(titleLabel);

    // 开机进入桌面模式
    QHBoxLayout *deskStartRow = new QHBoxLayout();
    deskStartRow->addWidget(new DLabel(tr("开机进入桌面模式"), this));
    deskStartRow->addStretch();
    m_desktopStartSwitch = new DSwitchButton(this);
    m_desktopStartSwitch->setToolTip(tr("启动后直接进入桌面模式"));
    deskStartRow->addWidget(m_desktopStartSwitch);
    layout->addLayout(deskStartRow);

    // 连续新增
    QHBoxLayout *contRow = new QHBoxLayout();
    contRow->addWidget(new DLabel(tr("快速录入连续新增"), this));
    contRow->addStretch();
    m_continuousAddSwitch = new DSwitchButton(this);
    m_continuousAddSwitch->setToolTip(tr("保存后不关闭窗口，可连续录入"));
    contRow->addWidget(m_continuousAddSwitch);
    layout->addLayout(contRow);

    // 默认颜色
    QHBoxLayout *colorRow = new QHBoxLayout();
    colorRow->addWidget(new DLabel(tr("便签默认颜色"), this));
    colorRow->addStretch();
    m_defaultColorCombo = new QComboBox(this);
    m_defaultColorCombo->addItem(tr("蓝色"), "#409EFF");
    m_defaultColorCombo->addItem(tr("绿色"), "#67C23A");
    m_defaultColorCombo->addItem(tr("黄色"), "#E6A23C");
    m_defaultColorCombo->addItem(tr("橙色"), "#F56C6C");
    m_defaultColorCombo->addItem(tr("紫色"), "#B37FEB");
    m_defaultColorCombo->addItem(tr("红色"), "#F56C6C");
    m_defaultColorCombo->setFixedWidth(200);
    colorRow->addWidget(m_defaultColorCombo);
    layout->addLayout(colorRow);

    // 透明度
    QHBoxLayout *opacityRow = new QHBoxLayout();
    opacityRow->addWidget(new DLabel(tr("桌面便签透明度"), this));
    opacityRow->addStretch();
    m_opacitySlider = new QSlider(Qt::Horizontal, this);
    m_opacitySlider->setRange(40, 95);
    m_opacitySlider->setValue(90);
    m_opacitySlider->setFixedWidth(200);
    DLabel *opacityLabel = new DLabel("90%", this);
    opacityLabel->setFixedWidth(36);
    connect(m_opacitySlider, &QSlider::valueChanged, this, [opacityLabel](int v) {
        opacityLabel->setText(QString::number(v) + "%");
    });
    opacityRow->addWidget(m_opacitySlider);
    opacityRow->addWidget(opacityLabel);
    layout->addLayout(opacityRow);

    // 最多便签数
    QHBoxLayout *maxRow = new QHBoxLayout();
    maxRow->addWidget(new DLabel(tr("最多便签数"), this));
    maxRow->addStretch();
    m_maxNotesSpin = new QSpinBox(this);
    m_maxNotesSpin->setRange(1, 12);
    m_maxNotesSpin->setValue(6);
    m_maxNotesSpin->setSuffix(tr(" 张"));
    maxRow->addWidget(m_maxNotesSpin);
    layout->addLayout(maxRow);

    // Wayland 提示
    DLabel *waylandTip = new DLabel(tr("💡 Wayland 下嵌入桌面受限，便签默认停靠右侧"), this);
    waylandTip->setStyleSheet("color: palette(placeholderText); font-size: 11px; padding: 8px 0;");
    layout->addWidget(waylandTip);

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

    // 引擎选择后只显示对应的 Key 输入框
    QStackedWidget *keyStack = new QStackedWidget(this);

    QWidget *deepseekPage = new QWidget(this);
    QVBoxLayout *dsLayout = new QVBoxLayout(deepseekPage);
    dsLayout->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout *dsRow = new QHBoxLayout();
    dsRow->addWidget(new DLabel(tr("DeepSeek API Key"), this));
    dsRow->addStretch();
    m_deepseekKeyEdit = new QLineEdit(this);
    m_deepseekKeyEdit->setEchoMode(QLineEdit::Password);
    m_deepseekKeyEdit->setFixedWidth(300);
    dsRow->addWidget(m_deepseekKeyEdit);
    dsLayout->addLayout(dsRow);
    keyStack->addWidget(deepseekPage);

    QWidget *tongyiPage = new QWidget(this);
    QVBoxLayout *tyLayout = new QVBoxLayout(tongyiPage);
    tyLayout->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout *tyRow = new QHBoxLayout();
    tyRow->addWidget(new DLabel(tr("通义千问 API Key"), this));
    tyRow->addStretch();
    m_tongyiKeyEdit = new QLineEdit(this);
    m_tongyiKeyEdit->setEchoMode(QLineEdit::Password);
    m_tongyiKeyEdit->setFixedWidth(300);
    tyRow->addWidget(m_tongyiKeyEdit);
    tyLayout->addLayout(tyRow);
    keyStack->addWidget(tongyiPage);

    keyStack->setCurrentIndex(0);
    layout->addWidget(keyStack);

    connect(m_aiEngineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [keyStack, this](int idx) {
        keyStack->setCurrentIndex(idx);
        QSettings().setValue("ai/engine", idx == 0 ? "DeepSeek" : "通义千问");
    });

    QHBoxLayout *testRow = new QHBoxLayout();
    testRow->addStretch();
    m_aiTestBtn = new DPushButton(tr("测试连接"), this);
    testRow->addWidget(m_aiTestBtn);
    layout->addLayout(testRow);

    QLabel *tip = new QLabel(tr("💡 API Key 将加密存储在本地，不会上传"), this);
    tip->setStyleSheet("color:palette(placeholderText); font-size:11px; padding:4px 0;");
    layout->addWidget(tip);

    connect(m_aiTestBtn, &DPushButton::clicked, this, [this]() {
        auto *app = ShorthandApplication::instance();
        auto *ai = app ? app->aiService() : nullptr;
        if (!ai) {
            DDialog d(this);
            d.setTitle(tr("测试连接"));
            d.setMessage(tr("AI 服务未初始化"));
            d.addButton(tr("确定"));
            d.exec();
            return;
        }
        m_aiTestBtn->setEnabled(false);
        m_aiTestBtn->setText(tr("测试中..."));
        // 使用输入框中的明文（loadSettings 已归一化解密），并用当前引擎实测
        QString key = m_aiEngineCombo->currentIndex() == 0
            ? m_deepseekKeyEdit->text() : m_tongyiKeyEdit->text();
        if (key.trimmed().isEmpty()) {
            m_aiTestBtn->setEnabled(true);
            m_aiTestBtn->setText(tr("测试连接"));
            DDialog d(this);
            d.setTitle(tr("测试连接"));
            d.setMessage(tr("请先输入 API Key"));
            d.addButton(tr("确定"));
            d.exec();
            return;
        }
        QUrl url(m_aiEngineCombo->currentIndex() == 0
            ? QString("https://api.deepseek.com/v1/models")
            : QString("https://dashscope.aliyuncs.com/api/v1/models"));
        QNetworkAccessManager *mgr = new QNetworkAccessManager(this);
        QNetworkRequest req(url);
        req.setRawHeader("Authorization", ("Bearer " + key.trimmed()).toUtf8());
        QNetworkReply *reply = mgr->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, mgr]() {
            reply->deleteLater();
            mgr->deleteLater();
            m_aiTestBtn->setEnabled(true);
            m_aiTestBtn->setText(tr("测试连接"));
            bool ok = reply->error() == QNetworkReply::NoError;
            DDialog d(this);
            d.setTitle(tr("测试连接"));
            d.setMessage(ok ? tr("✅ 连接成功，API Key 有效") : tr("❌ 连接失败：%1").arg(reply->errorString()));
            d.addButton(tr("确定"));
            d.exec();
        });
    });

    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createAsrPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    QHBoxLayout *engineRow = new QHBoxLayout();
    engineRow->addWidget(new DLabel(tr("语音识别引擎"), this));
    engineRow->addStretch();
    m_asrEngineCombo = new QComboBox(this);
    m_asrEngineCombo->addItem(tr("离线语音 (Whisper)"));
    m_asrEngineCombo->addItem(tr("百度语音"));
    m_asrEngineCombo->addItem(tr("讯飞语音"));
    m_asrEngineCombo->addItem(tr("阿里云语音"));
    m_asrEngineCombo->setFixedWidth(200);
    engineRow->addWidget(m_asrEngineCombo);
    layout->addLayout(engineRow);

    // 在线引擎 API Key 配置区（离线时隐藏）
    QWidget *credentialWidget = new QWidget(this);
    QVBoxLayout *credLayout = new QVBoxLayout(credentialWidget);
    credLayout->setContentsMargins(0, 0, 0, 0);
    credLayout->setSpacing(8);

    auto addKeyRow = [this, credLayout](const QString &label, QLineEdit *&edit) {
        QHBoxLayout *row = new QHBoxLayout();
        row->addWidget(new DLabel(label, this));
        row->addStretch();
        edit = new QLineEdit(this);
        edit->setEchoMode(QLineEdit::Password);
        edit->setFixedWidth(280);
        row->addWidget(edit);
        credLayout->addLayout(row);
    };

    QLabel *baiduTitle = new QLabel(tr("百度语音"), this);
    baiduTitle->setStyleSheet("font-weight:600; padding:4px 0;");
    credLayout->addWidget(baiduTitle);
    addKeyRow(tr("API Key"), m_baiduAsrKey);
    addKeyRow(tr("Secret Key"), m_baiduAsrSecret);

    QLabel *xunfeiTitle = new QLabel(tr("讯飞语音"), this);
    xunfeiTitle->setStyleSheet("font-weight:600; padding:4px 0;");
    credLayout->addWidget(xunfeiTitle);
    addKeyRow(tr("APP ID"), m_xunfeiAsrAppId);
    addKeyRow(tr("API Key"), m_xunfeiAsrKey);
    addKeyRow(tr("API Secret"), m_xunfeiAsrSecret);

    QLabel *aliTitle = new QLabel(tr("阿里云语音"), this);
    aliTitle->setStyleSheet("font-weight:600; padding:4px 0;");
    credLayout->addWidget(aliTitle);
    addKeyRow(tr("Access Key ID"), m_aliyunAsrKey);
    addKeyRow(tr("Access Key Secret"), m_aliyunAsrSecret);

    QHBoxLayout *testRow = new QHBoxLayout();
    testRow->addStretch();
    m_asrTestBtn = new DPushButton(tr("测试连接"), this);
    testRow->addWidget(m_asrTestBtn);
    credLayout->addLayout(testRow);

    layout->addWidget(credentialWidget);

    QLabel *tip = new QLabel(tr("💡 离线模式：本地运行无需联网，首次使用前请确保已下载模型文件\n💡 在线模式：百度 ai.baidu.com | 讯飞 xfyun.cn | 阿里云 nls.aliyun.com"), this);
    tip->setStyleSheet("color:palette(placeholderText); font-size:11px; padding:4px 0;");
    layout->addWidget(tip);

    // 切换引擎时显示/隐藏 API Key 配置
    auto updateCredVisibility = [this, credentialWidget]() {
        QString cur = m_asrEngineCombo->currentText();
        bool offline = cur.contains("离线") || cur.contains("Whisper");
        credentialWidget->setVisible(!offline);
    };
    connect(m_asrEngineCombo, &QComboBox::currentTextChanged, this, updateCredVisibility);
    updateCredVisibility();

    connect(m_asrTestBtn, &DPushButton::clicked, this, [this]() {
        DDialog d(this);
        d.setTitle(tr("提示"));
        d.setMessage(tr("ASR 连接测试功能将在后续版本中实现。"));
        d.addButton(tr("确定"));
        d.exec();
    });

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

    QLabel *tip = new QLabel(tr("点击输入框后直接按下新的快捷键组合（如 Ctrl+Shift+N），Esc 清除"), this);
    tip->setStyleSheet("color:palette(placeholderText); font-size:11px;");
    layout->addWidget(tip);
    // 点击输入框后捕获按键组合
    m_shortcutEdit->installEventFilter(this);

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

    QString language = settings.value("appearance/language", QStringLiteral("zh_CN")).toString();
    int langIdx = m_languageCombo->findData(language);
    if (langIdx >= 0) m_languageCombo->setCurrentIndex(langIdx);

    m_recordingDirEdit->setText(settings.value("recording/storage_dir").toString());

    m_lastShortcut = settings.value("shortcut/quick_entry", SHORTCUT_QUICK_ENTRY).toString();
    m_shortcutEdit->setText(m_lastShortcut);
    m_compactStartSwitch->setChecked(settings.value("startup/compact_mode", false).toBool());

    // Desktop mode settings
    m_desktopStartSwitch->setChecked(settings.value("desktop/start_in_desktop_mode", false).toBool());
    m_continuousAddSwitch->setChecked(settings.value("desktop/continuous_add", false).toBool());

    QString defaultColor = settings.value("desktop/default_color", "#409EFF").toString();
    int colorIdx = m_defaultColorCombo->findData(defaultColor);
    if (colorIdx >= 0) m_defaultColorCombo->setCurrentIndex(colorIdx);

    m_opacitySlider->setValue(settings.value("desktop/opacity", 90).toInt());
    m_maxNotesSpin->setValue(settings.value("desktop/max_notes", 6).toInt());

    // AI settings
    QString engine = settings.value("ai/engine", "DeepSeek").toString();
    int idx = m_aiEngineCombo->findText(engine);
    if (idx >= 0) m_aiEngineCombo->setCurrentIndex(idx);
    m_deepseekKeyEdit->setText(CryptoUtil::decryptDeep(settings.value("ai/deepseek_key").toString()));
    m_tongyiKeyEdit->setText(CryptoUtil::decryptDeep(settings.value("ai/tongyi_key").toString()));

    // ASR settings
    QString asrEngine = settings.value("asr/engine", "离线语音 (Whisper)").toString();
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

void SettingsDialog::saveSettings()
{
    QSettings settings;

    // General
    settings.setValue("appearance/theme", m_themeCombo->currentIndex());

    // 语言（IDE-201 P4-T9）：语言切换需重启后生效
    const QString newLanguage = m_languageCombo->currentData().toString();
    const QString oldLanguage = settings.value("appearance/language", QStringLiteral("zh_CN")).toString();
    settings.setValue("appearance/language", newLanguage);
    if (newLanguage != oldLanguage) {
        DDialog d(this);
        d.setTitle(tr("语言已更改"));
        d.setMessage(tr("界面语言已切换，重启应用后生效。"));
        d.addButton(tr("确定"));
        d.exec();
    }
    // 快捷键：只保存有效的组合，无效/空值回退为默认 Alt+Space
    const QString shortcut = normalizedShortcut(m_shortcutEdit->text());
    m_lastShortcut = shortcut.isEmpty() ? QString(SHORTCUT_QUICK_ENTRY) : shortcut;
    settings.setValue("shortcut/quick_entry", m_lastShortcut);
    m_shortcutEdit->setText(m_lastShortcut);
    settings.setValue("startup/compact_mode", m_compactStartSwitch->isChecked());

    // Desktop mode settings
    settings.setValue("desktop/start_in_desktop_mode", m_desktopStartSwitch->isChecked());
    settings.setValue("desktop/continuous_add", m_continuousAddSwitch->isChecked());
    settings.setValue("desktop/default_color", m_defaultColorCombo->currentData().toString());
    settings.setValue("desktop/opacity", m_opacitySlider->value());
    settings.setValue("desktop/max_notes", m_maxNotesSpin->value());

    // AI settings
    settings.setValue("ai/engine", m_aiEngineCombo->currentText());
    // AI Key 统一由 setApiKeyForEngine 加密存储（内部会归一化已加密的历史值）

    // 实时更新 AI 服务
    auto *app = ShorthandApplication::instance();
    if (auto *ai = app->aiService()) {
        ai->setEngine(AiServiceManager::engineFromName(m_aiEngineCombo->currentText()));
        ai->setApiKeyForEngine(AiServiceManager::DeepSeek, m_deepseekKeyEdit->text());
        ai->setApiKeyForEngine(AiServiceManager::Tongyi, m_tongyiKeyEdit->text());
    }

    // 录音存储目录
    settings.setValue("recording/storage_dir", m_recordingDirEdit->text().trimmed());

    // ASR settings
    settings.setValue("asr/engine", m_asrEngineCombo->currentText());
    settings.setValue("asr/baidu_key", m_baiduAsrKey->text());
    settings.setValue("asr/baidu_secret", m_baiduAsrSecret->text());
    settings.setValue("asr/xunfei_appid", m_xunfeiAsrAppId->text());
    settings.setValue("asr/xunfei_key", m_xunfeiAsrKey->text());
    settings.setValue("asr/xunfei_secret", m_xunfeiAsrSecret->text());
    settings.setValue("asr/aliyun_key", m_aliyunAsrKey->text());
    settings.setValue("asr/aliyun_secret", m_aliyunAsrSecret->text());

    // 实时更新 ASR 服务（重新加载凭据，保证重新配置后立即生效）
    if (auto *asr = app->asrService()) {
        asr->setEngine(AsrServiceManager::engineFromName(m_asrEngineCombo->currentText()));
        asr->reloadCredentials();
    }

    // 应用主题
    int themeIdx = m_themeCombo->currentIndex();
    auto *themeHelper = DGuiApplicationHelper::instance();
    if (themeHelper) {
        switch (themeIdx) {
        case 0: themeHelper->setPaletteType(DGuiApplicationHelper::UnknownType); break;
        case 1: themeHelper->setPaletteType(DGuiApplicationHelper::LightType); break;
        case 2: themeHelper->setPaletteType(DGuiApplicationHelper::DarkType); break;
        }
    }

    // 自启动
    QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    QString desktopFile = autostartDir + "/uos-shorthand.desktop";
    if (m_autostartSwitch->isChecked()) {
        QDir().mkpath(autostartDir);
        QFile f(desktopFile);
        if (!f.exists()) {
            if (f.open(QIODevice::WriteOnly)) {
                QTextStream out(&f);
                out.setEncoding(QStringConverter::Utf8);
                out << "[Desktop Entry]\nType=Application\nName=UOS速记\nExec=uos-shorthand\nX-GNOME-Autostart-enabled=true\n";
                f.close();
            }
        }
    } else {
        QFile::remove(desktopFile);
    }
}

bool SettingsDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_shortcutEdit) {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            // 仅在无修饰键时 Esc 才作为“清除”处理；带修饰键的组合继续捕获
            if (keyEvent->key() == Qt::Key_Escape
                    && !(keyEvent->modifiers() & (Qt::ControlModifier | Qt::AltModifier
                                                  | Qt::ShiftModifier | Qt::MetaModifier))) {
                m_shortcutEdit->clear();
                return true;
            }
            if (keyEvent->key() == Qt::Key_unknown || keyEvent->text().isEmpty()) {
                // 纯修饰键按下：先清空，等待最终按键
                m_shortcutEdit->clear();
                return true;
            }
            // 组装按键组合并显示
            int modifiers = keyEvent->modifiers()
                    & (Qt::ControlModifier | Qt::AltModifier
                       | Qt::ShiftModifier | Qt::MetaModifier);
            m_shortcutEdit->setText(QKeySequence(modifiers | keyEvent->key())
                                    .toString(QKeySequence::PortableText));
            return true;
        }
        if (event->type() == QEvent::FocusOut) {
            // 失焦时若未录入有效组合，恢复为上一个有效快捷键
            if (m_shortcutEdit->text().trimmed().isEmpty() && !m_lastShortcut.isEmpty()) {
                m_shortcutEdit->setText(m_lastShortcut);
            }
            return false;
        }
        // 其它事件（Hide/Show/Paint 等）只关心按键捕获，直接放行即可。
        return false;
    }
    // 其它对象直接放行，不再转发：
    // ① 本对话框只对 m_shortcutEdit 装了事件过滤器，其余对象无需处理；
    // ② 原代码 return SettingsDialog::eventFilter(watched, event) 是“自己递归自己”
    //    （并非调用 DTK 基类），Release 下被编译器优化成死循环 jmp self，
    //    导致：构造期 QTabWidget 插入页面的 Hide 事件、启动期 DDialog 内容过滤器
    //    转发的 QLabel Show/Hide 事件全部卡死（IDE-192 启动挂起）。
    // ③ 也不转发给 DDialog::eventFilter：DTK 基类处理 Hide 事件同样会死循环。
    return false;
}

QString SettingsDialog::normalizedShortcut(const QString &text) const
{
    const QString t = text.trimmed();
    if (t.isEmpty()) return QString();
    const QKeySequence seq(t);
    if (seq.isEmpty()) return QString();
    // 至少需要一个非修饰键（纯 Ctrl/Alt/Shift/Meta 组合无效）
    const int key = seq[0].key();
    if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt
            || key == Qt::Key_Meta) {
        return QString();
    }
    return seq.toString(QKeySequence::PortableText);
}
