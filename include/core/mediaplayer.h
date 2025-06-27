#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class AudioDecoder;
class VideoDecoder;

enum class MediaType {
    None,
    Audio,
    Video
};

enum class PlaybackState {
    Stopped,
    Playing,
    Paused
};

class MediaPlayer : public QObject {
    Q_OBJECT

public:
    explicit MediaPlayer(QObject *parent = nullptr);
    ~MediaPlayer();

    // File operations
    bool loadFile(const QString &filePath);
    void close();
    bool isValidMediaFile(const QString &filePath);
    bool hasVideoStream(const QString &filePath);
    bool hasAudioStream(const QString &filePath);

    // Playback control
    void play();
    void pause();
    void stop();
    void seek(qint64 position);

    // State queries
    bool isPlaying() const;
    qint64 getPosition() const;
    MediaType mediaType() const { return m_mediaType; }
    PlaybackState state() const { return m_state; }
    qint64 getDuration() const { return m_duration; }
    QString getCurrentFile() const { return m_currentFile; }
    bool hasVideo() const { return m_hasVideo; }
    bool hasAudio() const { return m_hasAudio; }

signals:
    void fileLoaded(const QString &filePath);
    void frameReady(const QImage &frame);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void playbackStateChanged(bool isPlaying);
    void audioPositionChanged(qint64 position);
    void audioPlaybackStateChanged(bool isPlaying);
    void error(const QString &message);

private slots:
    void onVideoFrameReady(const QImage &frame);

private:
    void setState(PlaybackState state);

    MediaType m_mediaType;
    PlaybackState m_state;
    QString m_currentFile;
    qint64 m_duration;
    qint64 m_position;
    bool m_hasVideo;
    bool m_hasAudio;

    AudioDecoder* m_audioDecoder;
    VideoDecoder* m_videoDecoder;
    QTimer m_positionTimer;
}; 