#include "core/audiodecoder.h"
#include "media/ffmpegwrapper.h"
#include <QAudioOutput>
#include <QAudioSink>
#include <QMediaDevices>
#include <QIODevice>
#include <QBuffer>
#include <QTimer>
#include <QDebug>
#include <QThread>
#include "core/mediaplayer.h"
#include "core/videodecoder.h"

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

AudioDecoder::~AudioDecoder() {
    qDebug() << "AudioDecoder::~AudioDecoder() called";
    if (m_audioTimer) {
        m_audioTimer->stop();
        m_audioTimer->deleteLater();
        m_audioTimer = nullptr;
    }
    QThread::msleep(100); // Даем таймеру завершиться
}

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
    
    if (m_audioTimer) {
        m_audioTimer->stop();
        m_audioTimer->deleteLater();
        m_audioTimer = nullptr;
    }
    
    m_position = 0;
    m_duration = 0;
    emit durationChanged(0);
    emit positionChanged(0);
}

void AudioDecoder::play()
{
    qDebug() << "AudioDecoder::play: called - open:" << m_ffmpeg->isOpen() << "playing:" << m_isPlaying;
    
    if (!m_ffmpeg->isOpen() || m_isPlaying) {
        qDebug() << "AudioDecoder::play: not ready to play";
        return;
    }
    
    // Start playback from current position
    if (!m_ffmpeg->startPlayback(m_position)) {
        qDebug() << "Failed to start audio playback";
        return;
    }
    
    // Убеждаемся, что аудиоустройство готово
    if (!m_audioSink || !m_audioDevice) {
        qDebug() << "AudioDecoder::play: setting up audio output";
        setupAudioOutput();
    }
    
    // Дополнительная проверка состояния QAudioSink
    if (m_audioSink && m_audioSink->state() == QAudio::StoppedState) {
        qDebug() << "AudioDecoder::play: QAudioSink is stopped, restarting...";
        m_audioDevice = m_audioSink->start();
        if (!m_audioDevice) {
            qDebug() << "AudioDecoder::play: failed to restart QAudioSink";
            return;
        }
    }
    
    if (m_audioSink && m_audioDevice) {
        qDebug() << "AudioDecoder::play: starting audio sink and immediate audio writing";
        
        // Убеждаемся, что устройство открыто и готово к записи
        if (!m_audioDevice->isOpen()) {
            qDebug() << "AudioDecoder::play: QIODevice is not open, restarting...";
            m_audioDevice = m_audioSink->start();
            if (!m_audioDevice || !m_audioDevice->isOpen()) {
                qDebug() << "AudioDecoder::play: failed to restart QIODevice";
                return;
            }
        }
        
        m_isPlaying = true;
        m_positionTimer.start();
        
        // Немедленно начинаем запись аудио, не ждём таймера
        qDebug() << "AudioDecoder::play: starting immediate audio thread";
        startAudioThread();
        
        qDebug() << "Audio playback started at" << m_position << "ms";
        emit playbackStateChanged(true);
    } else {
        qDebug() << "AudioDecoder::play: failed to get audio sink or device";
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
    
    if (m_audioTimer) {
        m_audioTimer->stop();
        m_audioTimer->deleteLater();
        m_audioTimer = nullptr;
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
    qDebug() << "setupAudioOutput: starting";
    
    // Очищаем предыдущие устройства
    if (m_audioDevice) {
        m_audioDevice->close();
        m_audioDevice = nullptr;
    }
    
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink->deleteLater();
        m_audioSink = nullptr;
    }
    
    // Get default audio device
    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        qDebug() << "No audio output device found";
        return;
    }
    
    qDebug() << "setupAudioOutput: got default device:" << device.description();
    
    // Create audio format
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);
    
    if (!device.isFormatSupported(format)) {
        qDebug() << "Audio format not supported, using default";
        format = device.preferredFormat();
    }
    
    qDebug() << "setupAudioOutput: using format - sample rate:" << format.sampleRate()
             << "channels:" << format.channelCount()
             << "format:" << format.sampleFormat();
    
    // Синхронизируем формат с FFmpeg
    syncFFmpegAudioFormat(format);
    
    // Create audio output using QAudioSink
    m_audioSink = new QAudioSink(device, format, this);
    if (!m_audioSink) {
        qDebug() << "setupAudioOutput: failed to create QAudioSink";
        return;
    }
    
    // Устанавливаем буфер большего размера для стабильности
    m_audioSink->setBufferSize(32768); // Увеличиваем до 32KB
    
    qDebug() << "setupAudioOutput: QAudioSink created, starting...";
    m_audioDevice = m_audioSink->start();
    
    if (!m_audioDevice) {
        qDebug() << "Failed to start audio device";
        return;
    }
    
    // Добавляем небольшую задержку для инициализации
    QThread::msleep(50);
    
    // Проверяем состояние QIODevice
    qDebug() << "setupAudioOutput: QIODevice state after start:";
    qDebug() << "  isOpen:" << m_audioDevice->isOpen();
    qDebug() << "  isWritable:" << m_audioDevice->isWritable();
    qDebug() << "  isSequential:" << m_audioDevice->isSequential();
    
    qDebug() << "setupAudioOutput: QIODevice started successfully";
    qDebug() << "Audio output setup: sample rate" << format.sampleRate()
             << "channels" << format.channelCount()
             << "format" << format.sampleFormat();
}

void AudioDecoder::startAudioThread()
{
    qDebug() << "startAudioThread: starting audio timer with safety checks";
    if (m_audioTimer) {
        m_audioTimer->stop();
        m_audioTimer->deleteLater();
        m_audioTimer = nullptr;
    }
    
    m_audioTimer = new QTimer(this);
    connect(m_audioTimer, &QTimer::timeout, this, &AudioDecoder::scheduleNextAudioChunk);
    m_audioTimer->start(20); // Возвращаем к 20ms для более частой подачи данных
    qDebug() << "startAudioThread: audio timer started with 20ms interval";
}

void AudioDecoder::scheduleNextAudioChunk()
{
    if (!m_isPlaying) {
        qDebug() << "scheduleNextAudioChunk: not playing, stopping";
        if (m_audioTimer) {
            m_audioTimer->stop();
        }
        return;
    }
    
    // Дополнительные проверки перед записью
    if (!m_ffmpeg) {
        qDebug() << "scheduleNextAudioChunk: FFmpegWrapper is null!";
        if (m_audioTimer) {
            m_audioTimer->stop();
        }
        return;
    }
    
    if (!m_audioSink) {
        qDebug() << "scheduleNextAudioChunk: QAudioSink is null!";
        if (m_audioTimer) {
            m_audioTimer->stop();
        }
        return;
    }
    
    // Проверяем состояние QAudioSink
    if (m_audioSink->state() == QAudio::StoppedState) {
        qDebug() << "scheduleNextAudioChunk: QAudioSink is stopped, restarting...";
        m_audioDevice = m_audioSink->start();
        if (!m_audioDevice) {
            qDebug() << "scheduleNextAudioChunk: failed to restart QAudioSink";
            if (m_audioTimer) {
                m_audioTimer->stop();
            }
            return;
        }
    }
    
    if (!m_audioDevice) {
        qDebug() << "scheduleNextAudioChunk: QIODevice is null, recreating...";
        m_audioDevice = m_audioSink->start();
        if (m_audioDevice) {
            qDebug() << "scheduleNextAudioChunk: QIODevice recreated successfully";
        } else {
            qDebug() << "scheduleNextAudioChunk: failed to recreate QIODevice";
            return;
        }
    }
    
    // Получаем позицию видео через MediaPlayer
    qint64 videoPos = 0;
    MediaPlayer* player = qobject_cast<MediaPlayer*>(parent());
    if (player && player->hasVideo()) {
        VideoDecoder* vdec = player->getVideoDecoder();
        if (vdec) videoPos = vdec->getPosition();
    }
    qint64 audioPos = m_ffmpeg->getCurrentTime();
    qint64 avDiff = audioPos - videoPos;
    qDebug() << "A/V sync diff:" << avDiff << "ms (audioPos:" << audioPos << ", videoPos:" << videoPos << ")";

    // Если аудио сильно отстает от видео, ускоряем его
    if (avDiff < -100) {
        // Пропускаем несколько аудиофреймов для догона
        for (int i = 0; i < 3; ++i) {
            m_ffmpeg->getNextAudioData(1024);
        }
        return;
    }
    
    // Если аудио опережает видео более чем на 80 мс, подаем тишину
    if (avDiff > 80) {
        QByteArray silence(1024 * 2 * 2, 0); // 1024 сэмпла * 2 канала * 2 байта
        m_audioDevice->write(silence);
        return;
    }
    
    // Получаем аудиоданные с улучшенной буферизацией
    QByteArray audioData;
    const int chunksToBuffer = 2; // Возвращаем к 2 чанкам для лучшей буферизации
    
    for (int i = 0; i < chunksToBuffer; ++i) {
        QByteArray chunk = m_ffmpeg->getNextAudioData(1024);
        if (!chunk.isEmpty()) {
            audioData.append(chunk);
        } else {
            // Если не можем получить данные, добавляем тишину для непрерывности
            QByteArray silence(1024 * 2 * 2, 0); // 1024 сэмпла * 2 канала * 2 байта
            audioData.append(silence);
        }
    }
    
    if (audioData.isEmpty()) {
        qDebug() << "scheduleNextAudioChunk: no audio data available";
        static int emptyDataCount = 0;
        emptyDataCount++;
        if (emptyDataCount > 20) {
            qDebug() << "scheduleNextAudioChunk: too many empty data calls, stopping";
            stop();
            emptyDataCount = 0;
        }
        return;
    }
    
    // Сбрасываем счетчик пустых данных
    static int emptyDataCount = 0;
    emptyDataCount = 0;
    
    qDebug() << "scheduleNextAudioChunk: got" << audioData.size() << "bytes (buffered)";
    
    // Проверяем, что m_audioDevice всё ещё валиден
    qDebug() << "scheduleNextAudioChunk: QIODevice state: isOpen=" << m_audioDevice->isOpen() << ", isWritable=" << m_audioDevice->isWritable();
    
    // Если устройство закрыто, пытаемся его перезапустить
    if (!m_audioDevice->isOpen()) {
        qDebug() << "scheduleNextAudioChunk: QIODevice is not open, restarting...";
        m_audioDevice = m_audioSink->start();
        if (!m_audioDevice || !m_audioDevice->isOpen()) {
            qDebug() << "scheduleNextAudioChunk: failed to restart QIODevice";
            if (m_audioTimer) {
                m_audioTimer->stop();
            }
            return;
        }
    }
    
    if (!m_audioDevice->isWritable()) {
        qDebug() << "scheduleNextAudioChunk: QIODevice is not writable!";
        // Попробуем подождать немного и повторить
        QThread::msleep(10);
        if (!m_audioDevice->isWritable()) {
            qDebug() << "scheduleNextAudioChunk: QIODevice still not writable after wait, stopping timer";
            if (m_audioTimer) {
                m_audioTimer->stop();
            }
            return;
        } else {
            qDebug() << "scheduleNextAudioChunk: QIODevice became writable after wait";
        }
    }
    
    // Записываем аудиоданные по частям для предотвращения переполнения буфера
    qint64 totalWritten = 0;
    const int maxChunkSize = 4096; // Максимальный размер чанка для записи
    
    while (totalWritten < audioData.size()) {
        int currentChunkSize = qMin(maxChunkSize, audioData.size() - totalWritten);
        QByteArray chunk = audioData.mid(totalWritten, currentChunkSize);
        
        qint64 written = m_audioDevice->write(chunk);
        if (written < 0) {
            qDebug() << "scheduleNextAudioChunk: error writing audio chunk:" << written;
            // Если произошла ошибка записи, пытаемся перезапустить устройство
            m_audioDevice = m_audioSink->start();
            if (m_audioDevice && m_audioDevice->isWritable()) {
                written = m_audioDevice->write(chunk);
                if (written < 0) {
                    qDebug() << "scheduleNextAudioChunk: error writing after restart:" << written;
                    break;
                }
            } else {
                break;
            }
        } else if (written != chunk.size()) {
            qDebug() << "scheduleNextAudioChunk: partial chunk write:" << written << "of" << chunk.size() << "bytes";
            totalWritten += written;
            // Небольшая пауза перед следующей попыткой
            QThread::msleep(1);
        } else {
            totalWritten += written;
        }
    }
    
    if (totalWritten != audioData.size()) {
        qDebug() << "scheduleNextAudioChunk: total written:" << totalWritten << "of" << audioData.size() << "bytes";
    } else {
        qDebug() << "scheduleNextAudioChunk: successfully wrote" << totalWritten << "bytes";
    }
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
