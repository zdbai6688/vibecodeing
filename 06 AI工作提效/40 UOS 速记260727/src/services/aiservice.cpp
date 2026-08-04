// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "aiservice.h"
#include "cryptoutil.h"

#include <QNetworkRequest>
#include <QUrl>
#include <QDebug>
#include <QSettings>
#include <QFile>
#include <QFileInfo>

// ============================================================
// DeepSeek Service
// ============================================================

static void lockSettingsFile()
{
    QSettings settings;
    QString filePath = settings.fileName();
    QFile file(filePath);
    if (file.exists()) {
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    }
}

DeepSeekService::DeepSeekService(const QString &apiKey, QObject *parent)
    : QObject(parent), m_apiKey(apiKey)
{
    m_network = new QNetworkAccessManager(this);
}

QJsonObject DeepSeekService::buildRequestBody(const AiCompletionRequest &req) const
{
    QJsonObject body;
    body["model"] = "deepseek-chat";
    body["temperature"] = req.temperature;
    body["max_tokens"] = req.maxTokens;
    body["stream"] = req.stream;

    QJsonArray msgs;
    for (const auto &msg : req.messages) {
        QJsonObject m;
        m["role"] = msg.role;
        m["content"] = msg.content;
        msgs.append(m);
    }
    body["messages"] = msgs;
    return body;
}

AiCompletionResult DeepSeekService::parseResponse(const QByteArray &data) const
{
    AiCompletionResult result;
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        result.success = false;
        result.errorMessage = "Invalid JSON response";
        return result;
    }

    QJsonObject obj = doc.object();
    if (obj.contains("error")) {
        QJsonObject errObj = obj["error"].toObject();
        result.success = false;
        result.errorMessage = errObj["message"].toString();
        if (result.errorMessage.isEmpty()) {
            result.errorMessage = QJsonDocument(errObj).toJson(QJsonDocument::Compact);
        }
        return result;
    }

    QJsonArray choices = obj["choices"].toArray();
    if (choices.isEmpty()) {
        result.success = false;
        result.errorMessage = "No choices in response";
        return result;
    }

    result.success = true;
    result.content = choices[0].toObject()["message"].toObject()["content"].toString();
    return result;
}

void DeepSeekService::complete(const AiCompletionRequest &req,
                                std::function<void(const AiCompletionResult &)> callback)
{
    if (m_apiKey.isEmpty()) {
        AiCompletionResult result;
        result.success = false;
        result.errorMessage = "DeepSeek API Key 未配置，请在设置中输入 API Key";
        callback(result);
        return;
    }

    QUrl url("https://api.deepseek.com/chat/completions");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    QJsonObject body = buildRequestBody(req);
    QNetworkReply *reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [reply, callback, this]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            AiCompletionResult result;
            result.success = false;
            // 尝试从响应体中提取详细错误信息
            QByteArray responseData = reply->readAll();
            QJsonDocument errDoc = QJsonDocument::fromJson(responseData);
            if (errDoc.isObject() && errDoc.object().contains("error")) {
                result.errorMessage = errDoc.object()["error"].toObject()["message"].toString();
            }
            if (result.errorMessage.isEmpty()) {
                result.errorMessage = reply->errorString();
            }
            callback(result);
            return;
        }
        QByteArray data = reply->readAll();
        callback(parseResponse(data));
    });
}

// ============================================================
// 通义千问 Service
// ============================================================

TongyiService::TongyiService(const QString &apiKey, QObject *parent)
    : QObject(parent), m_apiKey(apiKey)
{
    m_network = new QNetworkAccessManager(this);
}

QJsonObject TongyiService::buildRequestBody(const AiCompletionRequest &req) const
{
    QJsonObject body;
    body["model"] = "qwen-turbo";

    QJsonObject input;
    QJsonArray msgs;
    for (const auto &msg : req.messages) {
        QJsonObject m;
        m["role"] = msg.role;
        m["content"] = msg.content;
        msgs.append(m);
    }
    input["messages"] = msgs;
    body["input"] = input;

    QJsonObject params;
    params["temperature"] = req.temperature;
    params["max_tokens"] = req.maxTokens;
    body["parameters"] = params;
    return body;
}

AiCompletionResult TongyiService::parseResponse(const QByteArray &data) const
{
    AiCompletionResult result;
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        result.success = false;
        result.errorMessage = "无效的 JSON 响应";
        return result;
    }

    QJsonObject obj = doc.object();
    if (obj.contains("code") && obj["code"].toInt() != 200) {
        result.success = false;
        result.errorMessage = obj["message"].toString();
        if (result.errorMessage.isEmpty()) {
            result.errorMessage = QString("错误码: %1").arg(obj["code"].toInt());
        }
        return result;
    }

    QJsonObject output = obj["output"].toObject();
    QJsonArray choices = output["choices"].toArray();
    if (choices.isEmpty()) {
        result.success = false;
        result.errorMessage = "响应中无有效内容";
        return result;
    }

    result.success = true;
    result.content = choices[0].toObject()["message"].toObject()["content"].toString();
    return result;
}

void TongyiService::complete(const AiCompletionRequest &req,
                              std::function<void(const AiCompletionResult &)> callback)
{
    if (m_apiKey.isEmpty()) {
        AiCompletionResult result;
        result.success = false;
        result.errorMessage = "通义千问 API Key 未配置，请在设置中输入 API Key";
        callback(result);
        return;
    }

    QUrl url("https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    QJsonObject body = buildRequestBody(req);
    QNetworkReply *reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [reply, callback, this]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            AiCompletionResult result;
            result.success = false;
            QByteArray responseData = reply->readAll();
            QJsonDocument errDoc = QJsonDocument::fromJson(responseData);
            if (errDoc.isObject()) {
                QJsonObject errObj = errDoc.object();
                result.errorMessage = errObj["message"].toString();
                if (result.errorMessage.isEmpty()) {
                    result.errorMessage = errObj["code"].toString();
                }
            }
            if (result.errorMessage.isEmpty()) {
                result.errorMessage = reply->errorString();
            }
            callback(result);
            return;
        }
        QByteArray data = reply->readAll();
        callback(parseResponse(data));
    });
}

// ============================================================
// AiServiceManager
// ============================================================

AiServiceManager::AiServiceManager(QObject *parent)
    : QObject(parent)
{
    lockSettingsFile();
    QSettings settings;
    m_deepseekKey = CryptoUtil::decryptDeep(settings.value("ai/deepseek_key").toString());
    m_tongyiKey = CryptoUtil::decryptDeep(settings.value("ai/tongyi_key").toString());
    QString engineName = settings.value("ai/engine", "DeepSeek").toString();
    m_currentEngine = engineFromName(engineName);
    ensureService();
}

void AiServiceManager::setEngine(Engine engine)
{
    if (m_currentEngine != engine) {
        m_currentEngine = engine;
        ensureService();
        lockSettingsFile();
        QSettings settings;
        settings.setValue("ai/engine", engineName(engine));
        emit engineChanged(engineName(engine));
    }
}

void AiServiceManager::setApiKey(const QString &key)
{
    setApiKeyForEngine(m_currentEngine, key);
}

void AiServiceManager::setApiKeyForEngine(Engine engine, const QString &key)
{
    // 兼容历史遗留的已加密/多层加密字符串：先归一化为明文，避免重复加密
    const QString plain = CryptoUtil::decryptDeep(key);
    lockSettingsFile();
    QSettings settings;
    if (engine == DeepSeek) {
        m_deepseekKey = plain;
        settings.setValue("ai/deepseek_key", CryptoUtil::encrypt(plain));
    } else {
        m_tongyiKey = plain;
        settings.setValue("ai/tongyi_key", CryptoUtil::encrypt(plain));
    }
    if (engine == m_currentEngine) {
        ensureService();
    }
}

void AiServiceManager::reloadCredentials()
{
    lockSettingsFile();
    QSettings settings;
    m_deepseekKey = CryptoUtil::decryptDeep(settings.value("ai/deepseek_key").toString());
    m_tongyiKey = CryptoUtil::decryptDeep(settings.value("ai/tongyi_key").toString());
    QString engineName = settings.value("ai/engine", "DeepSeek").toString();
    m_currentEngine = engineFromName(engineName);
    qInfo() << "AI 凭据已重新加载，引擎:" << engineName;
    ensureService();
}

IAiService *AiServiceManager::currentService() const
{
    return m_currentService;
}

QStringList AiServiceManager::availableEngines() const
{
    return {engineName(DeepSeek), engineName(Tongyi)};
}

void AiServiceManager::complete(const AiCompletionRequest &req,
                                 std::function<void(const AiCompletionResult &)> callback)
{
    if (!m_currentService) {
        AiCompletionResult result;
        result.success = false;
        result.errorMessage = "AI 服务未配置。请在设置中输入 API Key。";
        callback(result);
        return;
    }
    m_currentService->complete(req, callback);
}

void AiServiceManager::extractTodos(const QString &text,
                                     std::function<void(const QList<QPair<QString, int>> &)> callback)
{
    QString prompt = QString(
        "你是一个待办提取助手。请从以下文本中提取待办事项。\n"
        "请以JSON数组格式返回，每个元素包含title和priority(1-3)字段。\n"
        "例如：[{\"title\": \"完成报告\", \"priority\": 2}]\n"
        "如果没有任何待办，返回空数组[]。\n\n"
        "文本：\n%1"
    ).arg(text);

    AiCompletionRequest req;
    AiMessage userMsg;
    userMsg.role = "user";
    userMsg.content = prompt;
    req.messages.append(userMsg);
    req.temperature = 0.3;
    req.maxTokens = 1024;

    complete(req, [callback](const AiCompletionResult &result) {
        QList<QPair<QString, int>> todos;
        if (!result.success) {
            callback(todos);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(result.content.toUtf8());
        if (doc.isArray()) {
            for (const auto &item : doc.array()) {
                QJsonObject obj = item.toObject();
                QString title = obj["title"].toString();
                int priority = obj["priority"].toInt(2);
                if (!title.isEmpty()) {
                    todos.append({title, qBound(1, priority, 3)});
                }
            }
        }
        callback(todos);
    });
}

QString AiServiceManager::engineName(Engine e)
{
    switch (e) {
    case DeepSeek: return "DeepSeek";
    case Tongyi: return "通义千问";
    }
    return "DeepSeek";
}

AiServiceManager::Engine AiServiceManager::engineFromName(const QString &name)
{
    if (name == "通义千问" || name == "Tongyi") return Tongyi;
    return DeepSeek;
}

void AiServiceManager::ensureService()
{
    if (m_currentService) {
        delete m_currentService;
        m_currentService = nullptr;
    }

    QString key;
    if (m_currentEngine == DeepSeek) {
        key = m_deepseekKey;
    } else {
        key = m_tongyiKey;
    }

    m_currentService = createService(m_currentEngine, key);
}

IAiService *AiServiceManager::createService(Engine engine, const QString &apiKey)
{
    if (engine == DeepSeek) {
        return new DeepSeekService(apiKey, this);
    } else {
        return new TongyiService(apiKey, this);
    }
}
