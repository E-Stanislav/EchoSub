#pragma once

#include <QObject>
#include <QImage>
#include <QSize>
#include <QTimer>
#include <memory>

class FFmpegWrapper;

class VideoDecoder : public QObject {
    Q_OBJECT

public:
    explicit VideoDecoder(QObject *parent = nullptr);
    ~VideoDecoder();

    bool openFile(const QString &filePath);
    void close();

    void play();
    void pause();
    void stop();
    void seek(qint64 position);

    bool isPlaying() const;
    qint64 getPosition() const;
    qint64 getDuration() const;
    QSize getVideoSize() const;
    double frameRate() const { return m_frameRate; }

signals:
    void fileLoaded(const QString &filePath);
    void frameReady(const QImage &frame);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void playbackStateChanged(bool isPlaying);
    void error(const QString &message);

private slots:
    void updateFrame();

private:
    void setupTimer();
    void cleanup();

    std::unique_ptr<FFmpegWrapper> m_ffmpeg;
    QTimer m_timer;

    QString m_filePath;
    qint64 m_duration;
    qint64 m_position;
    QSize m_videoSize;
    double m_frameRate;
    bool m_isPlaying;
    bool m_isPaused;
    int m_currentFrame;
}; 