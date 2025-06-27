#include "core/mediaplayer.h"
#include "core/audiodecoder.h"
#include "core/videodecoder.h"
#include "media/ffmpegwrapper.h"
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QDebug>
#include <QTimer>

MediaPlayer::MediaPlayer(QObject *parent)
    : QObject(parent)
    , m_audioDecoder(nullptr)
    , m_videoDecoder(nullptr)
    , m_mediaType(MediaType::None)
    , m_state(PlaybackState::Stopped)
    , m_position(0)
    , m_duration(0)
    , m_hasVideo(false)
    , m_hasAudio(false)
{
    // Remove the connect for updatePosition as it's no longer used
}

MediaPlayer::~MediaPlayer()
{
    qDebug() << "MediaPlayer::~MediaPlayer() called";
    close();
}

bool MediaPlayer::loadFile(const QString &filePath)
{
    close();
    if (!isValidMediaFile(filePath)) {
        emit error("Unsupported file format");
        return false;
    }
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        emit error("File does not exist");
        return false;
    }
    
    m_currentFile = filePath;
    
    // Determine media type and initialize appropriate decoder
    if (hasVideoStream(filePath)) {
        m_mediaType = MediaType::Video;
        m_videoDecoder = new VideoDecoder(this);
        connect(m_videoDecoder, &VideoDecoder::frameReady, this, &MediaPlayer::frameReady);
        connect(m_videoDecoder, &VideoDecoder::positionChanged, this, &MediaPlayer::positionChanged);
        connect(m_videoDecoder, &VideoDecoder::durationChanged, this, &MediaPlayer::durationChanged);
        connect(m_videoDecoder, &VideoDecoder::playbackStateChanged, this, &MediaPlayer::playbackStateChanged);
        connect(m_videoDecoder, &VideoDecoder::error, this, &MediaPlayer::error);
        
        if (!m_videoDecoder->openFile(filePath)) {
            emit error("Failed to open video file");
            return false;
        }
        
        m_hasVideo = true;
        m_duration = m_videoDecoder->getDuration();
        
        // If video has audio, also initialize audio decoder
        if (hasAudioStream(filePath)) {
            m_audioDecoder = new AudioDecoder(this);
            connect(m_audioDecoder, &AudioDecoder::positionChanged, this, &MediaPlayer::audioPositionChanged);
            connect(m_audioDecoder, &AudioDecoder::playbackStateChanged, this, &MediaPlayer::audioPlaybackStateChanged);
            connect(m_audioDecoder, &AudioDecoder::error, this, &MediaPlayer::error);
            
            if (m_audioDecoder->openFile(filePath)) {
                m_hasAudio = true;
            }
        }
        
    } else if (hasAudioStream(filePath)) {
        m_mediaType = MediaType::Audio;
        m_audioDecoder = new AudioDecoder(this);
        connect(m_audioDecoder, &AudioDecoder::positionChanged, this, &MediaPlayer::positionChanged);
        connect(m_audioDecoder, &AudioDecoder::durationChanged, this, &MediaPlayer::durationChanged);
        connect(m_audioDecoder, &AudioDecoder::playbackStateChanged, this, &MediaPlayer::playbackStateChanged);
        connect(m_audioDecoder, &AudioDecoder::error, this, &MediaPlayer::error);
        
        if (!m_audioDecoder->openFile(filePath)) {
            emit error("Failed to open audio file");
            return false;
        }
        
        m_hasAudio = true;
        m_duration = m_audioDecoder->getDuration();
        
    } else {
        emit error("File contains neither video nor audio");
        return false;
    }
    
    emit fileLoaded(filePath);
    emit durationChanged(m_duration);
    return true;
}

void MediaPlayer::close()
{
    stop();
    if (m_audioDecoder) {
        m_audioDecoder->close();
        m_audioDecoder = nullptr;
    }
    if (m_videoDecoder) {
        m_videoDecoder->close();
        m_videoDecoder = nullptr;
    }
    m_mediaType = MediaType::None;
    m_currentFile.clear();
    m_duration = 0;
    m_position = 0;
    m_hasVideo = false;
    m_hasAudio = false;
    emit durationChanged(0);
    emit positionChanged(0);
}

void MediaPlayer::play()
{
    qDebug() << "MediaPlayer::play: mediaType:" << (int)m_mediaType << "hasVideo:" << m_hasVideo << "hasAudio:" << m_hasAudio;
    
    if (m_mediaType == MediaType::Audio && m_audioDecoder) {
        qDebug() << "MediaPlayer::play: playing audio only";
        m_audioDecoder->play();
        setState(PlaybackState::Playing);
    } else if (m_mediaType == MediaType::Video) {
        qDebug() << "MediaPlayer::play: playing video";
        if (m_videoDecoder) {
            m_videoDecoder->play();
        }
        
        // Для видео файлов с аудио также воспроизводим аудио с небольшой задержкой
        if (m_hasAudio && m_audioDecoder) {
            qDebug() << "MediaPlayer::play: also playing audio";
            // Уменьшаем задержку для лучшей синхронизации
            QTimer::singleShot(100, [this]() {
                if (m_audioDecoder) {
                    m_audioDecoder->play();
                }
            });
        }
        
        setState(PlaybackState::Playing);
    }
}

void MediaPlayer::pause()
{
    qDebug() << "MediaPlayer::pause: mediaType:" << (int)m_mediaType << "hasVideo:" << m_hasVideo << "hasAudio:" << m_hasAudio;
    
    if (m_mediaType == MediaType::Audio && m_audioDecoder) {
        m_audioDecoder->pause();
        setState(PlaybackState::Paused);
    } else if (m_mediaType == MediaType::Video) {
        if (m_videoDecoder) {
            m_videoDecoder->pause();
        }
        
        // Для видео файлов с аудио также останавливаем аудио
        if (m_hasAudio && m_audioDecoder) {
            m_audioDecoder->pause();
        }
        
        setState(PlaybackState::Paused);
    }
}

void MediaPlayer::stop()
{
    qDebug() << "MediaPlayer::stop: mediaType:" << (int)m_mediaType << "hasVideo:" << m_hasVideo << "hasAudio:" << m_hasAudio;
    
    if (m_mediaType == MediaType::Audio && m_audioDecoder) {
        m_audioDecoder->stop();
    } else if (m_mediaType == MediaType::Video) {
        if (m_videoDecoder) {
            m_videoDecoder->stop();
        }
        
        // Для видео файлов с аудио также останавливаем аудио
        if (m_hasAudio && m_audioDecoder) {
            m_audioDecoder->stop();
        }
    }
    setState(PlaybackState::Stopped);
}

void MediaPlayer::seek(qint64 position)
{
    if (m_mediaType == MediaType::Audio && m_audioDecoder) {
        m_audioDecoder->seek(position);
    } else if (m_mediaType == MediaType::Video && m_videoDecoder) {
        m_videoDecoder->seek(position);
    }
}

void MediaPlayer::onVideoFrameReady(const QImage &frame)
{
    emit frameReady(frame);
}

void MediaPlayer::setState(PlaybackState state)
{
    if (m_state != state) {
        m_state = state;
    }
}

bool MediaPlayer::hasVideoStream(const QString &filePath)
{
    // Use FFmpeg to check if file has video stream
    FFmpegWrapper tempFFmpeg;
    if (tempFFmpeg.openFile(filePath)) {
        bool hasVideo = tempFFmpeg.hasVideo();
        tempFFmpeg.close();
        return hasVideo;
    }
    return false;
}

bool MediaPlayer::hasAudioStream(const QString &filePath)
{
    // Use FFmpeg to check if file has audio stream
    FFmpegWrapper tempFFmpeg;
    if (tempFFmpeg.openFile(filePath)) {
        bool hasAudio = tempFFmpeg.hasAudio();
        tempFFmpeg.close();
        return hasAudio;
    }
    return false;
}

bool MediaPlayer::isValidMediaFile(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return false;
    }
    
    // Check if file has video or audio streams
    return hasVideoStream(filePath) || hasAudioStream(filePath);
}

qint64 MediaPlayer::getPosition() const
{
    if (m_videoDecoder) {
        return m_videoDecoder->getPosition();
    } else if (m_audioDecoder) {
        return m_audioDecoder->getPosition();
    }
    return m_position;
}

bool MediaPlayer::isPlaying() const
{
    if (m_videoDecoder) {
        return m_videoDecoder->isPlaying();
    } else if (m_audioDecoder) {
        return m_audioDecoder->isPlaying();
    }
    return false;
}
