#include "asrservice.h"
#include "whisper/whisperasrengine.h"

#include <QDebug>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QMap>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>

// ============================================================
// XfyunAsrEngine
// ============================================================

XfyunAsrEngine::XfyunAsrEngine(const QString &appid, const QString &apiKey,
                                 const QString &apiSecret, QObject *parent)
    : QObject(parent), m_appid(appid), m_apiKey(apiKey), m_apiSecret(apiSecret)
{
}

void XfyunAsrEngine::transcribe(const QString &audioFile,
                                  std::function<void(const AsrResult &)> callback)
{
    AsrResult result;
    // 查找 xfyun_asr.js 脚本：开发目录 → 安装目录 → 同级目录
    QString scriptPath = QCoreApplication::applicationDirPath() + "/../src/services/xfyun_asr.js";
    if (!QFile::exists(scriptPath)) {
        scriptPath = QCoreApplication::applicationDirPath() + "/../share/uos-shorthand/services/xfyun_asr.js";
    }
    if (!QFile::exists(scriptPath)) {
        scriptPath = QCoreApplication::applicationDirPath() + "/xfyun_asr.js";
    }

    if (!QFile::exists(scriptPath) || !QFile::exists(audioFile)) {
        result.success = false;
        result.errorMessage = "找不到脚本文件或音频文件";
        callback(result);
        return;
    }

    QProcess *proc = new QProcess(this);
    proc->setProgram("node");
    proc->setArguments({scriptPath, m_appid, m_apiKey, m_apiSecret, audioFile});

    connect(proc, &QProcess::finished, this, [this, proc, callback](int exitCode) {
        proc->deleteLater();
        QByteArray output = proc->readAllStandardOutput();
        QByteArray errOutput = proc->readAllStandardError();

        AsrResult result;
        if (exitCode != 0) {
            result.success = false;
            result.errorMessage = QString::fromUtf8(errOutput);
            callback(result);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(output);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            result.success = obj["success"].toBool();
            result.text = obj["text"].toString();
            // 优先使用中文错误消息(message)，fallback到短代码(error)
            QString msg = obj["message"].toString();
            result.errorMessage = msg.isEmpty() ? obj["error"].toString() : msg;
        } else {
            result.success = false;
            result.errorMessage = "解析响应失败";
        }
        callback(result);
    });

    proc->start();
}

// ============================================================
// BaiduAsrEngine (stub)
// ============================================================

BaiduAsrEngine::BaiduAsrEngine(const QString &apiKey, const QString &secretKey,
                                 QObject *parent)
    : QObject(parent), m_apiKey(apiKey), m_secretKey(secretKey)
{
}

void BaiduAsrEngine::transcribe(const QString &audioFile,
                                  std::function<void(const AsrResult &)> callback)
{
    QFile file(audioFile);
    if (!file.open(QIODevice::ReadOnly)) {
        AsrResult result;
        result.success = false;
        result.errorMessage = "无法读取音频文件";
        callback(result);
        return;
    }
    QByteArray audioData = file.readAll();
    file.close();

    // 获取 access_token
    QString tokenUrl = QString("https://aip.baidubce.com/oauth/2.0/token?grant_type=client_credentials&client_id=%1&client_secret=%2")
                           .arg(m_apiKey, m_secretKey);

    QNetworkAccessManager *mgr = new QNetworkAccessManager(this);
    QNetworkReply *tokenReply = mgr->get(QNetworkRequest(QUrl(tokenUrl)));

    connect(tokenReply, &QNetworkReply::finished, this, [this, mgr, tokenReply, audioData, callback]() {
        tokenReply->deleteLater();
        QByteArray tokenData = tokenReply->readAll();
        QJsonDocument tokenDoc = QJsonDocument::fromJson(tokenData);
        QString token = tokenDoc.object()["access_token"].toString();

        if (token.isEmpty()) {
            AsrResult result;
            result.success = false;
            result.errorMessage = "获取百度 Token 失败";
            callback(result);
            mgr->deleteLater();
            return;
        }

        // 调用短语音识别 API
        QString url = "https://vop.baidu.com/server_api";
        QJsonObject body;
        body["format"] = "wav";
        body["rate"] = 16000;
        body["channel"] = 1;
        body["cuid"] = "UOS速记";
        body["token"] = token;
        body["dev_pid"] = 1537;
        body["speech"] = QString::fromUtf8(audioData.toBase64());
        body["len"] = audioData.size();

        QUrl qurl(url);
        QNetworkRequest request(qurl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply *asrReply = mgr->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

        connect(asrReply, &QNetworkReply::finished, this, [this, mgr, asrReply, callback]() {
            asrReply->deleteLater();
            QByteArray data = asrReply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject obj = doc.object();

            AsrResult result;
            int errNo = obj["err_no"].toInt();
            result.success = (errNo == 0);
            if (result.success) {
                QJsonArray arr = obj["result"].toArray();
                if (!arr.isEmpty()) {
                    result.text = arr[0].toString();
                }
            } else {
                result.errorMessage = obj["err_msg"].toString();
            }
            callback(result);
            mgr->deleteLater();
        });
    });
}

// ============================================================
// AliyunAsrEngine
// ============================================================

AliyunAsrEngine::AliyunAsrEngine(const QString &accessKeyId, const QString &accessKeySecret,
                                   QObject *parent)
    : QObject(parent), m_accessKeyId(accessKeyId), m_accessKeySecret(accessKeySecret)
{
}

static QString aliyunHmacSha1(const QString &key, const QString &data)
{
    QMessageAuthenticationCode code(QCryptographicHash::Sha1, key.toUtf8());
    code.addData(data.toUtf8());
    return QString::fromLatin1(code.result().toBase64());
}

static QByteArray aliyunPercentEncode(const QString &str)
{
    QByteArray bytes = str.toUtf8();
    QByteArray out;
    for (char c : bytes) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '-' || c == '_' || c == '.' || c == '~') {
            out.append(c);
        } else {
            out.append('%');
            out.append(QByteArray::number((unsigned char)c, 16).toUpper());
        }
    }
    return out;
}

void AliyunAsrEngine::transcribe(const QString &audioFile,
                                  std::function<void(const AsrResult &)> callback)
{
    AsrResult result;
    if (m_accessKeyId.isEmpty() || m_accessKeySecret.isEmpty()) {
        result.success = false;
        result.errorMessage = "阿里云 AccessKey 未配置，请在设置中填写";
        callback(result);
        return;
    }

    QFile file(audioFile);
    if (!file.open(QIODevice::ReadOnly)) {
        result.success = false;
        result.errorMessage = "无法读取音频文件";
        callback(result);
        return;
    }
    QByteArray audioData = file.readAll();
    file.close();

    // 1. 获取 Token（RPC 调用 CreateToken）
    QString nonce = QString::number(QDateTime::currentMSecsSinceEpoch());
    QString timestamp = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd'T'HH:mm:ss'Z'");

    QMap<QString, QString> params;
    params["AccessKeyId"] = m_accessKeyId;
    params["Action"] = "CreateToken";
    params["Format"] = "JSON";
    params["RegionId"] = "cn-shanghai";
    params["SignatureMethod"] = "HMAC-SHA1";
    params["SignatureNonce"] = nonce;
    params["SignatureVersion"] = "1.0";
    params["Timestamp"] = timestamp;
    params["Version"] = "2019-02-28";

    // 排序并构造待签名字符串
    QStringList keys = params.keys();
    std::sort(keys.begin(), keys.end());
    QString canonicalized;
    for (const QString &k : keys) {
        canonicalized += aliyunPercentEncode(k) + "=" + aliyunPercentEncode(params[k]) + "&";
    }
    canonicalized.chop(1);

    QString stringToSign = "GET&%2F&" + QUrl::toPercentEncoding(canonicalized);
    QString signature = aliyunHmacSha1(m_accessKeySecret + "&", stringToSign);

    QString tokenUrl = "https://nls-meta.cn-shanghai.aliyuncs.com/?Action=CreateToken"
                       + QString("&AccessKeyId=%1&Format=JSON&RegionId=cn-shanghai")
                             .arg(QUrl::toPercentEncoding(m_accessKeyId))
                       + QString("&SignatureMethod=HMAC-SHA1&SignatureNonce=%1").arg(nonce)
                       + "&SignatureVersion=1.0"
                       + QString("&Timestamp=%1").arg(QUrl::toPercentEncoding(timestamp))
                       + "&Version=2019-02-28"
                       + QString("&Signature=%1").arg(QUrl::toPercentEncoding(signature));

    QNetworkAccessManager *mgr = new QNetworkAccessManager(this);
    QNetworkReply *tokenReply = mgr->get(QNetworkRequest(QUrl(tokenUrl)));

    connect(tokenReply, &QNetworkReply::finished, this,
            [this, mgr, tokenReply, audioData, callback]() {
        tokenReply->deleteLater();
        QByteArray tokenData = tokenReply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(tokenData);
        QString token = doc.object()["Token"].toObject()["Id"].toString();

        if (token.isEmpty()) {
            AsrResult result;
            result.success = false;
            result.errorMessage = "阿里云 Token 获取失败，请检查 AccessKey";
            callback(result);
            mgr->deleteLater();
            return;
        }

        // 2. 调用一句话识别 REST API
        QString url = "https://nls-gateway.cn-shanghai.aliyuncs.com/stream/v1/asr?token=" + token;
        QJsonObject body;
        body["appkey"] = m_accessKeyId;
        body["format"] = "wav";
        body["sample_rate"] = 16000;
        body["enable_punctuation_prediction"] = true;
        body["enable_inverse_text_normalization"] = true;
        body["enable_voice_detection"] = false;
        body["audio"] = QString::fromUtf8(audioData.toBase64());

        QUrl qurl(url);
        QNetworkRequest request(qurl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply *asrReply = mgr->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

        connect(asrReply, &QNetworkReply::finished, this,
                [this, mgr, asrReply, callback]() {
            asrReply->deleteLater();
            QByteArray data = asrReply->readAll();
            QJsonDocument rdoc = QJsonDocument::fromJson(data);
            QJsonObject obj = rdoc.object();

            AsrResult result;
            int status = obj["status"].toInt(-1);
            result.success = (status == 20000000);
            if (result.success) {
                result.text = obj["result"].toString();
            } else {
                result.errorMessage = obj["message"].toString(QString::number(status));
            }
            callback(result);
            mgr->deleteLater();
        });
    });
}

// ============================================================
// 测试连接实现
// ============================================================

void BaiduAsrEngine::testConnection(std::function<void(bool success, const QString &message)> callback)
{
    QString tokenUrl = QString("https://aip.baidubce.com/oauth/2.0/token?grant_type=client_credentials&client_id=%1&client_secret=%2")
                           .arg(m_apiKey, m_secretKey);

    QNetworkAccessManager *mgr = new QNetworkAccessManager(this);
    QNetworkReply *reply = mgr->get(QNetworkRequest(QUrl(tokenUrl)));

    connect(reply, &QNetworkReply::finished, this, [reply, mgr, callback]() {
        reply->deleteLater();
        mgr->deleteLater();
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        if (obj.contains("access_token") && !obj["access_token"].toString().isEmpty()) {
            callback(true, QString::fromUtf8("连接成功，Access Token 已获取"));
        } else {
            QString err = obj["error_description"].toString();
            if (err.isEmpty()) err = obj["error"].toString();
            if (err.isEmpty()) err = QString::fromUtf8("未知错误");
            callback(false, QString::fromUtf8("连接失败：%1").arg(err));
        }
    });
}

void XfyunAsrEngine::testConnection(std::function<void(bool success, const QString &message)> callback)
{
    QString scriptPath = QCoreApplication::applicationDirPath() + "/../src/services/xfyun_asr.js";
    if (!QFile::exists(scriptPath)) {
        scriptPath = QCoreApplication::applicationDirPath() + "/../share/uos-shorthand/services/xfyun_asr.js";
    }
    if (!QFile::exists(scriptPath)) {
        scriptPath = QCoreApplication::applicationDirPath() + "/xfyun_asr.js";
    }

    if (!QFile::exists(scriptPath)) {
        callback(false, QString::fromUtf8("找不到 xfyun_asr.js 脚本文件"));
        return;
    }

    QProcess *proc = new QProcess(this);
    proc->setProgram("node");
    proc->setArguments({scriptPath, QStringLiteral("--test"), m_appid, m_apiKey, m_apiSecret});

    connect(proc, &QProcess::finished, this, [proc, callback]([[maybe_unused]] int exitCode) {
        proc->deleteLater();
        QByteArray output = proc->readAllStandardOutput();
        QByteArray errOutput = proc->readAllStandardError();

        QJsonDocument doc = QJsonDocument::fromJson(output);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            bool ok = obj["success"].toBool();
            QString msg = obj["message"].toString();
            if (msg.isEmpty()) msg = obj["error"].toString();
            callback(ok, msg);
        } else {
            callback(false, QString::fromUtf8("连接失败：%1").arg(QString::fromUtf8(errOutput)));
        }
    });

    proc->start();
}

void AliyunAsrEngine::testConnection(std::function<void(bool success, const QString &message)> callback)
{
    QString url = QString("https://nls-meta.cn-shanghai.aliyuncs.com/api/createToken?AccessKeyId=%1&KeySecret=%2")
                      .arg(m_accessKeyId, m_accessKeySecret);

    QNetworkAccessManager *mgr = new QNetworkAccessManager(this);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = mgr->post(request, QByteArray("{}"));

    connect(reply, &QNetworkReply::finished, this, [reply, mgr, callback]() {
        reply->deleteLater();
        mgr->deleteLater();
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        if (obj.contains("Token") && !obj["Token"].toString().isEmpty()) {
            callback(true, QString::fromUtf8("连接成功，Access Token 已获取"));
        } else {
            QString err = obj["ErrMsg"].toString();
            if (err.isEmpty()) err = obj["Message"].toString();
            if (err.isEmpty()) err = QString::fromUtf8("未知错误（请检查 AccessKey）");
            callback(false, QString::fromUtf8("连接失败：%1").arg(err));
        }
    });
}

// ============================================================
// AsrServiceManager
// ============================================================

AsrServiceManager::AsrServiceManager(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_xfyunAppid = settings.value("asr/xunfei_appid").toString();
    m_xfyunApiKey = settings.value("asr/xunfei_key").toString();
    m_xfyunApiSecret = settings.value("asr/xunfei_secret").toString();
    m_baiduApiKey = settings.value("asr/baidu_key").toString();
    m_baiduSecretKey = settings.value("asr/baidu_secret").toString();
    m_aliyunAccessKeyId = settings.value("asr/aliyun_key").toString();
    m_aliyunAccessKeySecret = settings.value("asr/aliyun_secret").toString();
    QString engineName = settings.value("asr/engine", "离线语音 (Whisper)").toString();
    m_currentEngine = engineFromName(engineName);
    ensureService();
}

void AsrServiceManager::setEngine(Engine engine)
{
    if (m_currentEngine != engine) {
        m_currentEngine = engine;
        ensureService();
        QSettings().setValue("asr/engine", engineName(engine));
        emit engineChanged(engineName(engine));
    }
}

void AsrServiceManager::setCredentials(Engine engine, const QString &key1, const QString &key2)
{
    QSettings settings;
    switch (engine) {
    case Xfyun:
        m_xfyunAppid = key1;
        m_xfyunApiKey = key2;
        settings.setValue("asr/xunfei_appid", key1);
        settings.setValue("asr/xunfei_key", key2);
        break;
    case Baidu:
        m_baiduApiKey = key1;
        m_baiduSecretKey = key2;
        settings.setValue("asr/baidu_key", key1);
        settings.setValue("asr/baidu_secret", key2);
        break;
    case Aliyun:
        m_aliyunAccessKeyId = key1;
        m_aliyunAccessKeySecret = key2;
        settings.setValue("asr/aliyun_key", key1);
        settings.setValue("asr/aliyun_secret", key2);
        break;
    default:
        break;
    }
    if (engine == m_currentEngine) {
        ensureService();
    }
}

void AsrServiceManager::reloadCredentials()
{
    QSettings settings;
    m_xfyunAppid = settings.value("asr/xunfei_appid").toString();
    m_xfyunApiKey = settings.value("asr/xunfei_key").toString();
    m_xfyunApiSecret = settings.value("asr/xunfei_secret").toString();
    m_baiduApiKey = settings.value("asr/baidu_key").toString();
    m_baiduSecretKey = settings.value("asr/baidu_secret").toString();
    m_aliyunAccessKeyId = settings.value("asr/aliyun_key").toString();
    m_aliyunAccessKeySecret = settings.value("asr/aliyun_secret").toString();
    // 凭据可能已变化，重建当前服务以使用最新配置
    ensureService();
}

IAsrEngine *AsrServiceManager::currentService() const
{
    return m_currentService;
}

void AsrServiceManager::transcribe(const QString &audioFile,
                                    std::function<void(const AsrResult &)> callback)
{
    if (!m_currentService) {
        AsrResult result;
        result.success = false;
        result.errorMessage = "ASR 引擎未配置，请在设置中配置 API Key";
        callback(result);
        return;
    }
    m_currentService->transcribe(audioFile, callback);
}
void AsrServiceManager::testEngine(Engine engine,
                                    std::function<void(bool success, const QString &message)> callback)
{
    // 如果是 Whisper，直接检查模型文件
    if (engine == Whisper) {
        QString modelPath = WhisperAsrEngine::defaultModelPath(WhisperAsrEngine::Tiny);
        if (QFile::exists(modelPath)) {
            callback(true, QString::fromUtf8("Whisper 模型文件已就绪"));
        } else {
            callback(false, QString::fromUtf8("Whisper 模型文件不存在，请将 ggml-tiny.bin 放到 %1").arg(modelPath));
        }
        return;
    }

    // 创建对应引擎的临时实例并调用 testConnection
    IAsrEngine *svc = nullptr;
    switch (engine) {
    case Xfyun:
        svc = new XfyunAsrEngine(m_xfyunAppid, m_xfyunApiKey, m_xfyunApiSecret, this);
        break;
    case Baidu:
        svc = new BaiduAsrEngine(m_baiduApiKey, m_baiduSecretKey, this);
        break;
    case Aliyun:
        svc = new AliyunAsrEngine(m_aliyunAccessKeyId, m_aliyunAccessKeySecret, this);
        break;
    default:
        callback(false, QString::fromUtf8("不支持的引擎"));
        return;
    }

    // 获取原始指针，在 lambda 中安全删除
    IAsrEngine *rawSvc = svc;
    rawSvc->testConnection([this, rawSvc, callback](bool ok, const QString &msg) {
        delete rawSvc;
        callback(ok, msg);
    });
}



QString AsrServiceManager::engineName(Engine e)
{
    switch (e) {
    case Whisper: return "离线语音 (Whisper)";
    case Xfyun: return "讯飞语音";
    case Baidu: return "百度语音";
    case Aliyun: return "阿里云语音";
    }
    return "离线语音 (Whisper)";
}

AsrServiceManager::Engine AsrServiceManager::engineFromName(const QString &name)
{
    if (name.contains("Whisper") || name.contains("离线")) return Whisper;
    if (name == "百度语音") return Baidu;
    if (name == "阿里云语音") return Aliyun;
    if (name == QStringLiteral("讯飞语音")) return Xfyun;
    return Whisper; // 默认使用离线引擎
}

void AsrServiceManager::ensureService()
{
    if (m_currentService) {
        delete m_currentService;
        m_currentService = nullptr;
    }
    m_currentService = createService(m_currentEngine);
}

IAsrEngine *AsrServiceManager::createService(Engine engine)
{
    switch (engine) {
    case Whisper:
        ensureWhisperModel();
        return new WhisperAsrEngine(
            WhisperAsrEngine::defaultModelPath(WhisperAsrEngine::Tiny), this);
    case Xfyun:
        return new XfyunAsrEngine(m_xfyunAppid, m_xfyunApiKey, m_xfyunApiSecret, this);
    case Baidu:
        return new BaiduAsrEngine(m_baiduApiKey, m_baiduSecretKey, this);
    case Aliyun:
        return new AliyunAsrEngine(m_aliyunAccessKeyId, m_aliyunAccessKeySecret, this);
    default:
        ensureWhisperModel();
        return new WhisperAsrEngine(
            WhisperAsrEngine::defaultModelPath(WhisperAsrEngine::Tiny), this);
    }
}

void AsrServiceManager::ensureWhisperModel()
{
    QString target = WhisperAsrEngine::defaultModelPath(WhisperAsrEngine::Tiny);
    if (QFile::exists(target)) return;

    // 从项目目录复制模型（开发环境），或提示下载
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/whisper/ggml-tiny.bin",
        QCoreApplication::applicationDirPath() + "/../whisper/ggml-tiny.bin",
        QCoreApplication::applicationDirPath() + "/ggml-tiny.bin",
    };
    for (const QString &src : candidates) {
        if (QFile::exists(src)) {
            QDir().mkpath(QFileInfo(target).absolutePath());
            if (QFile::copy(src, target)) {
                qInfo() << "Whisper 模型已复制到:" << target;
                return;
            }
        }
    }
    qWarning() << "未找到 Whisper 模型文件，首次使用请将 ggml-tiny.bin 放到:" << target;
}