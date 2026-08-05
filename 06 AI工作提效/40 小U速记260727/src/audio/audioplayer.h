#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QObject>
#include <QString>
#include <QElapsedTimer>

class QProcess;

/**
 * 基于 GStreamer playbin 的音频播放器
 * 支持：播放/暂停/停止/跳转到指定时间/获取当前播放位置
 */
class AudioPlayer : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(AudioPlayer)

public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer() override;

    bool load(const QString &filePath);
    bool play();
    bool pause();
    bool stop();
    bool seekTo(qint64 positionMs);
    bool setPosition(qint64 positionMs) { return seekTo(positionMs); }

    bool isLoaded() const { return m_loaded; }
    bool isPlaying() const { return m_playing; }
    qint64 positionMs() const;
    qint64 durationMs() const;
    QString currentFile() const { return m_filePath; }

signals:
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void playbackStarted();
    void playbackPaused();
    void playbackStopped();
    void playbackFinished();
    void errorOccurred(const QString &message);

private:
    bool m_loaded = false;
    bool m_playing = false;
    QString m_filePath;
    qint64 m_offsetMs = 0;
    qint64 m_playStartPos = 0;
    QProcess *m_playProc = nullptr;
    QElapsedTimer m_playTimer;
};

#endif // AUDIOPLAYER_H