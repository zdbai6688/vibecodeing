#ifndef ASRSERVICE_H
#define ASRSERVICE_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QJsonObject>
#include <functional>

struct AsrSegment {
    QString text;
    QString speaker;
    qint64 startMs = 0;
    qint64 endMs = 0;
};

struct AsrResult {
    bool success = false;
    QString text;
    QList<AsrSegment> segments;
    QString errorMessage;
};

// ASR 引擎抽象接口
class IAsrEngine
{
public:
    virtual ~IAsrEngine() = default;
    virtual QString name() const = 0;
    virtual void transcribe(const QString &audioFile,
                            std::function<void(const AsrResult &)> callback) = 0;
    // 测试连接：验证 API Key 和服务的可用性
    virtual void testConnection(std::function<void(bool success, const QString &message)> callback) = 0;
};

// 讯飞 ASR 引擎
class XfyunAsrEngine : public QObject, public IAsrEngine
{
    Q_OBJECT
public:
    explicit XfyunAsrEngine(const QString &appid, const QString &apiKey,
                            const QString &apiSecret, QObject *parent = nullptr);
    QString name() const override { return "讯飞语音"; }
    void transcribe(const QString &audioFile,
                    std::function<void(const AsrResult &)> callback) override;
    void testConnection(std::function<void(bool success, const QString &message)> callback) override;

private:
    QString m_appid;
    QString m_apiKey;
    QString m_apiSecret;
};

// 百度 ASR 引擎
class BaiduAsrEngine : public QObject, public IAsrEngine
{
    Q_OBJECT
public:
    explicit BaiduAsrEngine(const QString &apiKey, const QString &secretKey,
                            QObject *parent = nullptr);
    QString name() const override { return "百度语音"; }
    void transcribe(const QString &audioFile,
                    std::function<void(const AsrResult &)> callback) override;
    void testConnection(std::function<void(bool success, const QString &message)> callback) override;
private:
    QString m_apiKey;
    QString m_secretKey;
};

// 阿里云 ASR 引擎（一句话识别 REST API）
class AliyunAsrEngine : public QObject, public IAsrEngine
{
    Q_OBJECT
public:
    explicit AliyunAsrEngine(const QString &accessKeyId, const QString &accessKeySecret,
                             QObject *parent = nullptr);
    QString name() const override { return "阿里云语音"; }
    void transcribe(const QString &audioFile,
                    std::function<void(const AsrResult &)> callback) override;
    void testConnection(std::function<void(bool success, const QString &message)> callback) override;
private:
    QString m_accessKeyId;
    QString m_accessKeySecret;
};

// ASR 服务管理器
class AsrServiceManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(AsrServiceManager)

public:
    enum Engine { Whisper, Xfyun, Baidu, Aliyun };

    explicit AsrServiceManager(QObject *parent = nullptr);

    void setEngine(Engine engine);
    void setCredentials(Engine engine, const QString &key1, const QString &key2);
    // 从 QSettings 重新加载所有 ASR 凭据并重建当前服务（设置页保存后调用，避免凭据过期）
    void reloadCredentials();
    Engine currentEngine() const { return m_currentEngine; }
    IAsrEngine *currentService() const;

    void transcribe(const QString &audioFile,
                    std::function<void(const AsrResult &)> callback);

    // 测试指定引擎的连接，使用已保存的凭据
    void testEngine(Engine engine,
                    std::function<void(bool success, const QString &message)> callback);

    static QString engineName(Engine e);
    static Engine engineFromName(const QString &name);

signals:
    void engineChanged(const QString &engineName);

private:
    void ensureService();
    IAsrEngine *createService(Engine engine);
    void ensureWhisperModel();

    Engine m_currentEngine = Whisper;
    QString m_xfyunAppid;
    QString m_xfyunApiKey;
    QString m_xfyunApiSecret;
    QString m_baiduApiKey;
    QString m_baiduSecretKey;
    QString m_aliyunAccessKeyId;
    QString m_aliyunAccessKeySecret;
    IAsrEngine *m_currentService = nullptr;
};

#endif // ASRSERVICE_H
