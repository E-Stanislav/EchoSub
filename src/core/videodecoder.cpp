#include "core/videodecoder.h"
#include "media/ffmpegwrapper.h"
#include <QTimer>
#include <QDebug>

VideoDecoder::VideoDecoder(QObject *parent)
    : QObject(parent)
    , m_ffmpeg(std::make_unique<FFmpegWrapper>(this))
    , m_timer(QTimer(this))
    , m_position(0)
    , m_duration(0)
    , m_videoSize(0, 0)
    , m_frameRate(30.0)
    , m_isPlaying(false)
{
    connect(&m_timer, &QTimer::timeout, this, &VideoDecoder::updateFrame);
    
    // Connect FFmpeg error signals
    connect(m_ffmpeg.get(), &FFmpegWrapper::error, this, &VideoDecoder::error);
}

VideoDecoder::~VideoDecoder() {}

bool VideoDecoder::openFile(const QString &filePath)
{
    if (!m_ffmpeg->openFile(filePath)) {
        return false;
    }
    
    m_duration = m_ffmpeg->getVideoDuration();
    m_frameRate = m_ffmpeg->getVideoFrameRate();
    
    if (m_frameRate <= 0) {
        m_frameRate = 30.0; // Default frame rate
    }
    
    // Set timer interval based on frame rate
    int interval = static_cast<int>(1000.0 / m_frameRate);
    m_timer.setInterval(interval);
    
    qDebug() << "Video opened:" << filePath;
    qDebug() << "Duration:" << m_duration << "ms";
    qDebug() << "Frame rate:" << m_frameRate << "fps";
    qDebug() << "Timer interval:" << interval << "ms";
    
    emit fileLoaded(filePath);
    emit durationChanged(m_duration);
    
    return true;
}

void VideoDecoder::close()
{
    stop();
    m_ffmpeg->close();
    m_position = 0;
    m_duration = 0;
    emit durationChanged(0);
    emit positionChanged(0);
}

void VideoDecoder::play()
{
    if (!m_ffmpeg->isOpen() || m_isPlaying) {
        return;
    }
    
    // Start playback from current position
    if (!m_ffmpeg->startPlayback(m_position)) {
        qDebug() << "Failed to start playback";
        return;
    }
    
    // Принудительно читаем первый кадр для обновления позиции
    QImage firstFrame = m_ffmpeg->getNextFrame();
    if (!firstFrame.isNull()) {
        m_position = m_ffmpeg->getCurrentTime();
        emit frameReady(firstFrame);
        emit positionChanged(m_position);
        qDebug() << "VideoDecoder::play: read first frame, position:" << m_position << "ms";
    } else {
        qDebug() << "VideoDecoder::play: failed to read first frame, FFmpeg state:" << m_ffmpeg->isOpen();
    }
    
    m_isPlaying = true;
    m_timer.start();
    
    qDebug() << "Video playback started at" << m_position << "ms";
    emit playbackStateChanged(true);
}

void VideoDecoder::pause()
{
    if (!m_isPlaying) {
        return;
    }
    
    m_timer.stop();
    m_isPlaying = false;
    
    qDebug() << "Video playback paused at" << m_position << "ms";
    emit playbackStateChanged(false);
}

void VideoDecoder::stop()
{
    if (!m_isPlaying && m_position == 0) {
        return;
    }
    
    m_timer.stop();
    m_isPlaying = false;
    m_position = 0;
    
    qDebug() << "Video playback stopped";
    emit playbackStateChanged(false);
    emit positionChanged(0);
}

void VideoDecoder::seek(qint64 position)
{
    if (!m_ffmpeg->isOpen()) {
        return;
    }
    
    // Clamp position to valid range
    if (position < 0) position = 0;
    if (position > m_duration) position = m_duration;
    
    m_position = position;
    
    // If playing, restart playback from new position
    if (m_isPlaying) {
        m_timer.stop();
        if (m_ffmpeg->startPlayback(m_position)) {
            m_timer.start();
        }
    }
    
    qDebug() << "Video seeked to" << position << "ms";
    emit positionChanged(m_position);
}

void VideoDecoder::updateFrame()
{
    if (!m_isPlaying || !m_ffmpeg->isOpen()) {
        return;
    }
    
    // Get next frame
    QImage frame = m_ffmpeg->getNextFrame();
    
    if (frame.isNull()) {
        // End of stream or error
        qDebug() << "End of video stream or error getting frame";
        stop();
        return;
    }
    
    // Update position
    m_position = m_ffmpeg->getCurrentTime();
    
    // Emit frame and position
    emit frameReady(frame);
    emit positionChanged(m_position);
    
    // Check if we've reached the end
    if (m_position >= m_duration) {
        qDebug() << "Reached end of video";
        stop();
    }
}

QSize VideoDecoder::getVideoSize() const
{
    if (m_ffmpeg->isOpen()) {
        return QSize(m_ffmpeg->getVideoWidth(), m_ffmpeg->getVideoHeight());
    }
    return QSize(640, 480);
}

qint64 VideoDecoder::getDuration() const { return m_duration; }
qint64 VideoDecoder::getPosition() const { return m_position; }
bool VideoDecoder::isPlaying() const { return m_isPlaying; }
