#include "core/audiodecoder.h"
#include "media/ffmpegwrapper.h"
#include <QAudioOutput>
#include <QAudioSink>
#include <QMediaDevices>
#include <QIODevice>
#include <QTimer>
#include <QDebug>

AudioDecoder::AudioDecoder(QObject *parent)
    : QObject(parent)
    , m_ffmpeg(std::make_unique<FFmpegWrapper>(this))
    , m_audioSink(nullptr)
    , m_audioDevice(nullptr)
    , m_position(0)
    , m_duration(0)
    , m_isPlaying(false)
{
    connect(&m_positionTimer, &QTimer::timeout, this, &AudioDecoder::updatePosition);
    m_positionTimer.setInterval(100); // Update position every 100ms
    
    // Connect FFmpeg error signals
    connect(m_ffmpeg.get(), &FFmpegWrapper::error, this, &AudioDecoder::error);
}

AudioDecoder::~AudioDecoder() {}

bool AudioDecoder::openFile(const QString &filePath)
{
    if (!m_ffmpeg->openFile(filePath)) {
        return false;
    }
    
    if (!m_ffmpeg->hasAudio()) {
        qDebug() << "File does not contain audio";
        return false;
    }
    
    m_duration = m_ffmpeg->getAudioDuration();
    
    // Setup audio output
    setupAudioOutput();
    
    qDebug() << "Audio opened:" << filePath;
    qDebug() << "Duration:" << m_duration << "ms";
    qDebug() << "Sample rate:" << m_ffmpeg->getSampleRate();
    qDebug() << "Channels:" << m_ffmpeg->getChannels();
    
    emit fileLoaded(filePath);
    emit durationChanged(m_duration);
    
    return true;
}

void AudioDecoder::close()
{
    stop();
    m_ffmpeg->close();
    
    if (m_audioDevice) {
        m_audioDevice->close();
        m_audioDevice = nullptr;
    }
    
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink = nullptr;
    }
    
    m_position = 0;
    m_duration = 0;
    emit durationChanged(0);
    emit positionChanged(0);
}

void AudioDecoder::play()
{
    if (!m_ffmpeg->isOpen() || m_isPlaying) {
        return;
    }
    
    // Start playback from current position
    if (!m_ffmpeg->startPlayback(m_position)) {
        qDebug() << "Failed to start audio playback";
        return;
    }
    
    if (!m_audioSink || !m_audioDevice) {
        setupAudioOutput();
    }
    
    if (m_audioSink && m_audioDevice) {
        m_audioSink->start();
        m_isPlaying = true;
        m_positionTimer.start();
        
        // Start audio data thread
        startAudioThread();
        
        qDebug() << "Audio playback started at" << m_position << "ms";
        emit playbackStateChanged(true);
    }
}

void AudioDecoder::pause()
{
    if (!m_isPlaying) {
        return;
    }
    
    if (m_audioSink) {
        m_audioSink->suspend();
    }
    
    m_positionTimer.stop();
    m_isPlaying = false;
    
    qDebug() << "Audio playback paused at" << m_position << "ms";
    emit playbackStateChanged(false);
}

void AudioDecoder::stop()
{
    if (!m_isPlaying && m_position == 0) {
        return;
    }
    
    if (m_audioSink) {
        m_audioSink->stop();
    }
    
    m_positionTimer.stop();
    m_isPlaying = false;
    m_position = 0;
    
    qDebug() << "Audio playback stopped";
    emit playbackStateChanged(false);
    emit positionChanged(0);
}

void AudioDecoder::seek(qint64 position)
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
        stop();
        play();
    }
    
    qDebug() << "Audio seeked to" << position << "ms";
    emit positionChanged(m_position);
}

void AudioDecoder::syncFFmpegAudioFormat(const QAudioFormat& format) {
    AVSampleFormat ffmpegFmt = AV_SAMPLE_FMT_S16;
    switch (format.sampleFormat()) {
        case QAudioFormat::Int16: ffmpegFmt = AV_SAMPLE_FMT_S16; break;
        case QAudioFormat::Int32: ffmpegFmt = AV_SAMPLE_FMT_S32; break;
        case QAudioFormat::Float: ffmpegFmt = AV_SAMPLE_FMT_FLT; break;
        default: ffmpegFmt = AV_SAMPLE_FMT_S16; break;
    }
    m_ffmpeg->setOutputAudioFormat(format.sampleRate(), format.channelCount(), ffmpegFmt);
}

void AudioDecoder::setupAudioOutput()
{
    // Get default audio device
    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        qDebug() << "No audio output device found";
        return;
    }
    
    // Create audio format
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);
    
    if (!device.isFormatSupported(format)) {
        qDebug() << "Audio format not supported, using default";
        format = device.preferredFormat();
    }
    
    // Синхронизируем формат с FFmpeg
    syncFFmpegAudioFormat(format);
    
    // Create audio output
    m_audioSink = new QAudioSink(device, format, this);
    m_audioDevice = m_audioSink->start();
    
    if (!m_audioDevice) {
        qDebug() << "Failed to start audio device";
        return;
    }
    
    qDebug() << "Audio output setup: sample rate" << format.sampleRate()
             << "channels" << format.channelCount()
             << "format" << format.sampleFormat();
}

void AudioDecoder::startAudioThread()
{
    // This would ideally run in a separate thread
    // For now, we'll use a timer to periodically send audio data
    QTimer *audioTimer = new QTimer(this);
    connect(audioTimer, &QTimer::timeout, this, [this, audioTimer]() {
        if (!m_isPlaying || !m_audioDevice) {
            audioTimer->stop();
            return;
        }
        
        // Get audio data from FFmpeg
        QByteArray audioData = m_ffmpeg->getNextAudioData(4096);
        
        if (audioData.isEmpty()) {
            // End of stream
            qDebug() << "End of audio stream";
            stop();
            audioTimer->stop();
            return;
        }
        
        // Write audio data to device
        if (m_audioDevice->write(audioData) < 0) {
            qDebug() << "Error writing audio data";
        }
    });
    
    // Send audio data every 50ms (adjust based on buffer size)
    audioTimer->start(50);
}

void AudioDecoder::updatePosition()
{
    if (!m_isPlaying) {
        return;
    }
    
    // Update position from FFmpeg
    m_position = m_ffmpeg->getCurrentTime();
    emit positionChanged(m_position);
    
    // Check if we've reached the end
    if (m_position >= m_duration) {
        qDebug() << "Reached end of audio";
        stop();
    }
}
