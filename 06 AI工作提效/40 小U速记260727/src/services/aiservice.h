// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AISERVICE_H
#define AISERVICE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <functional>
#include <QList>
#include <QPair>

struct AiMessage {
    QString role;    // "system", "user", "assistant"
    QString content;
};

struct AiCompletionRequest {
    QList<AiMessage> messages;
    double temperature = 0.7;
    int maxTokens = 2048;
    bool stream = false;
};

struct AiCompletionResult {
    bool success = false;
    QString content;
    QString errorMessage;
};

// AI 服务抽象接口
class IAiService
{
public:
    virtual ~IAiService() = default;
    virtual QString name() const = 0;
    virtual void complete(const AiCompletionRequest &req,
                          std::function<void(const AiCompletionResult &)> callback) = 0;
};

// DeepSeek API 适配器
class DeepSeekService : public QObject, public IAiService
{
    Q_OBJECT
public:
    explicit DeepSeekService(const QString &apiKey, QObject *parent = nullptr);
    QString name() const override { return "DeepSeek"; }
    void complete(const AiCompletionRequest &req,
                  std::function<void(const AiCompletionResult &)> callback) override;

private:
    QJsonObject buildRequestBody(const AiCompletionRequest &req) const;
    AiCompletionResult parseResponse(const QByteArray &data) const;
    QString m_apiKey;
    QNetworkAccessManager *m_network;
};

// 通义千问 API 适配器
class TongyiService : public QObject, public IAiService
{
    Q_OBJECT
public:
    explicit TongyiService(const QString &apiKey, QObject *parent = nullptr);
    QString name() const override { return "通义千问"; }
    void complete(const AiCompletionRequest &req,
                  std::function<void(const AiCompletionResult &)> callback) override;

private:
    QJsonObject buildRequestBody(const AiCompletionRequest &req) const;
    AiCompletionResult parseResponse(const QByteArray &data) const;
    QString m_apiKey;
    QNetworkAccessManager *m_network;
};

// AI 服务管理器（工厂模式）
class AiServiceManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(AiServiceManager)

public:
    enum Engine { DeepSeek, Tongyi };

    explicit AiServiceManager(QObject *parent = nullptr);

    void setEngine(Engine engine);
    void setApiKey(const QString &key);
    void setApiKeyForEngine(Engine engine, const QString &key);
    /// 从 QSettings 重新加载所有 AI 凭据并重建当前服务（设置页保存后调用，避免凭据过期）
    void reloadCredentials();

    Engine currentEngine() const { return m_currentEngine; }
    IAiService *currentService() const;
    QStringList availableEngines() const;

    void complete(const AiCompletionRequest &req,
                  std::function<void(const AiCompletionResult &)> callback);

    void extractTodos(const QString &text,
                      std::function<void(const QList<QPair<QString, int>> &)> callback);

    static QString engineName(Engine e);
    static Engine engineFromName(const QString &name);

signals:
    void engineChanged(const QString &engineName);

private:
    void ensureService();
    IAiService *createService(Engine engine, const QString &apiKey);

    Engine m_currentEngine = DeepSeek;
    QString m_deepseekKey;
    QString m_tongyiKey;
    IAiService *m_currentService = nullptr;
};

#endif // AISERVICE_H
