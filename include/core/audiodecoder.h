#pragma once

#include <QObject>
#include <QAudioOutput>
#include <QAudioSink>
#include <QIODevice>
#include <QByteArray>
#include <QTimer>

class FFmpegWrapper;

class AudioDecoder : public QObject {
    Q_OBJECT

public:
    explicit AudioDecoder(QObject *parent = nullptr);
    ~AudioDecoder();

    bool openFile(const QString &filePath);
    void close();

    void play();
    void pause();
    void stop();
    void seek(qint64 position);

    bool isPlaying() const { return m_isPlaying; }
    qint64 getPosition() const { return m_position; }
    qint64 getDuration() const { return m_duration; }

signals:
    void fileLoaded(const QString &filePath);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void playbackStateChanged(bool isPlaying);
    void error(const QString &message);

private slots:
    void updatePosition();

private:
    void setupAudioOutput();
    void startAudioThread();
    void scheduleNextAudioChunk();
    void cleanup();
    void syncFFmpegAudioFormat(const QAudioFormat& format);

    std::unique_ptr<FFmpegWrapper> m_ffmpeg;
    QAudioSink *m_audioSink;
    QIODevice *m_audioDevice;
    QTimer m_positionTimer;
    QTimer* m_audioTimer = nullptr;

    QString m_filePath;
    qint64 m_duration;
    qint64 m_position;
    bool m_isPlaying;
    bool m_isPaused;
}; 