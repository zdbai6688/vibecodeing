#include "audioplayer.h"

#include <QProcess>
#include <QTimer>
#include <QDebug>
#include <QFileInfo>

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
{
    // 位置轮询定时器
    QTimer *pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, [this]() {
        if (m_playing) {
            qint64 pos = positionMs();
            emit positionChanged(pos);
        }
    });
    pollTimer->start(500);
}

AudioPlayer::~AudioPlayer()
{
    stop();
}

bool AudioPlayer::load(const QString &filePath)
{
    stop();
    m_filePath = filePath;
    m_loaded = QFileInfo::exists(filePath);
    if (!m_loaded) {
        emit errorOccurred(tr("音频文件不存在: %1").arg(filePath));
    }
    return m_loaded;
}

bool AudioPlayer::play()
{
    if (!m_loaded) return false;

    // 用 ffplay 播放，从当前偏移开始
    QProcess *proc = new QProcess(this);
    QStringList args;
    if (m_offsetMs > 0) {
        args << "-ss" << QString::number(m_offsetMs / 1000.0, 'f', 2);
    }
    args << "-nodisp" << "-autoexit" << m_filePath;

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        bool wasPlaying = m_playing;
        m_playing = false;
        if (wasPlaying && exitCode == 0) {
            emit playbackFinished();
        }
    });

    proc->start("ffplay", args);
    if (!proc->waitForStarted(3000)) {
        m_playing = false;
        emit errorOccurred(tr("无法启动播放器: %1").arg(proc->errorString()));
        proc->deleteLater();
        return false;
    }

    m_playProc = proc;
    m_playing = true;
    m_playStartPos = m_offsetMs;
    m_playTimer.restart();
    emit playbackStarted();
    return true;
}

bool AudioPlayer::pause()
{
    if (!m_playing) return false;
    m_offsetMs = positionMs();
    m_playing = false;
    if (m_playProc) {
        m_playProc->kill();
        m_playProc->waitForFinished(1000);
        m_playProc->deleteLater();
        m_playProc = nullptr;
    }
    emit playbackPaused();
    return true;
}

bool AudioPlayer::stop()
{
    m_offsetMs = 0;
    m_playing = false;
    if (m_playProc) {
        m_playProc->kill();
        m_playProc->waitForFinished(1000);
        m_playProc->deleteLater();
        m_playProc = nullptr;
    }
    emit playbackStopped();
    return true;
}

bool AudioPlayer::seekTo(qint64 positionMs)
{
    if (positionMs < 0) positionMs = 0;
    bool wasPlaying = m_playing;
    m_offsetMs = positionMs;
    if (wasPlaying) {
        if (m_playProc) {
            m_playProc->kill();
            m_playProc->waitForFinished(1000);
            m_playProc->deleteLater();
            m_playProc = nullptr;
        }
        m_playing = false;
        play();
    }
    emit positionChanged(positionMs);
    return true;
}

qint64 AudioPlayer::positionMs() const
{
    if (m_playing) {
        return m_playStartPos + m_playTimer.elapsed();
    }
    return m_offsetMs;
}

qint64 AudioPlayer::durationMs() const
{
    return 0;
}