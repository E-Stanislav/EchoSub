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
    
    // Получаем текущую позицию видео для синхронизации
    qint64 videoPosition = 0;
    MediaPlayer* player = qobject_cast<MediaPlayer*>(parent());
    if (player && player->hasVideo()) {
        VideoDecoder* vdec = player->getVideoDecoder();
        if (vdec) {
            videoPosition = vdec->getPosition();
            qDebug() << "AudioDecoder::play: video position:" << videoPosition << "ms, audio position:" << m_position << "ms";
        }
    }
    
    // Синхронизируем аудио позицию с видео позицией
    if (videoPosition > 0 && abs(videoPosition - m_position) > 100) {
        qDebug() << "AudioDecoder::play: syncing audio position to video position:" << videoPosition << "ms";
        m_position = videoPosition;
    }
    
    // Start playback from current position
    if (!m_ffmpeg->startPlayback(m_position)) {
        qDebug() << "Failed to start audio playback";
        return;
    }
    
    // Принудительно обновляем позицию в FFmpeg после паузы
    qDebug() << "AudioDecoder::play: forcing FFmpeg to seek to position:" << m_position << "ms";
    if (!m_ffmpeg->seekToTime(m_position)) {
        qDebug() << "AudioDecoder::play: failed to seek FFmpeg to position";
        return;
    }
    
    // Убеждаемся, что аудиоустройство готово
    if (!m_audioSink || !m_audioDevice) {
        qDebug() << "AudioDecoder::play: setting up audio output";
        setupAudioOutput();
    }
    
    // Проверяем состояние QAudioSink и восстанавливаем его если нужно
    if (m_audioSink) {
        qDebug() << "AudioDecoder::play: QAudioSink state:" << m_audioSink->state();
        if (m_audioSink->state() == QAudio::SuspendedState) {
            qDebug() << "AudioDecoder::play: resuming from suspended state";
            m_audioSink->resume();
            // Даем время на восстановление
            QThread::msleep(50);
        } else if (m_audioSink->state() == QAudio::StoppedState) {
            qDebug() << "AudioDecoder::play: QAudioSink is stopped, restarting...";
            m_audioDevice = m_audioSink->start();
            if (!m_audioDevice) {
                qDebug() << "AudioDecoder::play: failed to restart QAudioSink";
                return;
            }
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
    
    // Останавливаем аудио таймер
    if (m_audioTimer) {
        m_audioTimer->stop();
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
    m_audioSink->setBufferSize(65536); // Увеличиваем до 64KB для лучшей стабильности
    
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
        qDebug() << "startAudioThread: stopping existing audio timer";
        m_audioTimer->stop();
        m_audioTimer->deleteLater();
        m_audioTimer = nullptr;
    }
    
    m_audioTimer = new QTimer(this);
    connect(m_audioTimer, &QTimer::timeout, this, &AudioDecoder::scheduleNextAudioChunk);
    m_audioTimer->start(20); // Возвращаем к 20ms для более частой подачи данных
    qDebug() << "startAudioThread: audio timer started with 20ms interval, timer active:" << m_audioTimer->isActive();
}

void AudioDecoder::scheduleNextAudioChunk()
{
    static int callCount = 0;
    callCount++;
    if (callCount % 100 == 0) { // Логируем каждые 100 вызовов (примерно раз в 2 секунды)
        qDebug() << "scheduleNextAudioChunk: called, isPlaying:" << m_isPlaying << "callCount:" << callCount;
    }
    
    if (!m_isPlaying || !m_audioTimer || !m_audioTimer->isActive()) {
        if (m_audioTimer) {
            m_audioTimer->stop();
        }
        return;
    }
    
    // Дополнительные проверки перед записью
    if (!m_ffmpeg || !m_audioSink) {
        if (m_audioTimer) {
            m_audioTimer->stop();
        }
        return;
    }
    
    // Проверяем состояние QAudioSink и восстанавливаем его если нужно
    if (m_audioSink->state() == QAudio::SuspendedState) {
        qDebug() << "scheduleNextAudioChunk: resuming from suspended state";
        m_audioSink->resume();
        QThread::msleep(50);
    } else if (m_audioSink->state() == QAudio::StoppedState) {
        m_audioDevice = m_audioSink->start();
        if (!m_audioDevice) {
            if (m_audioTimer) {
                m_audioTimer->stop();
            }
            return;
        }
    }
    
    if (!m_audioDevice) {
        m_audioDevice = m_audioSink->start();
        if (!m_audioDevice) {
            return;
        }
    }
    
    // Получаем позицию видео через MediaPlayer только если есть видео
    qint64 videoPos = 0;
    bool hasVideo = false;
    bool videoEnded = false;
    MediaPlayer* player = qobject_cast<MediaPlayer*>(parent());
    if (player && player->hasVideo()) {
        VideoDecoder* vdec = player->getVideoDecoder();
        if (vdec) {
            videoPos = vdec->getPosition();
            hasVideo = true;
            // Проверяем, не закончилось ли видео
            if (videoPos == 0 && m_position > 1000) {
                videoEnded = true;
            }
        }
    }
    
    qint64 audioPos = m_ffmpeg->getCurrentTime();
    
    // Если видео закончилось, останавливаем аудио
    if (videoEnded) {
        qDebug() << "scheduleNextAudioChunk: video ended, stopping audio";
        stop();
        return;
    }
    
    // A/V синхронизация только для файлов с видео И только если видео активно воспроизводится
    if (hasVideo && videoPos > 0) {
        qint64 avDiff = audioPos - videoPos;
        
        // Логируем только значительные расхождения (увеличиваем порог)
        if (abs(avDiff) > 100) {
            qDebug() << "A/V sync diff:" << avDiff << "ms (audioPos:" << audioPos << ", videoPos:" << videoPos << ")";
        }

        // Если расхождение слишком большое (>200ms), принудительно синхронизируем
        if (abs(avDiff) > 200) {
            qDebug() << "scheduleNextAudioChunk: large A/V sync diff detected, forcing sync to video position:" << videoPos << "ms";
            if (m_ffmpeg->seekToTime(videoPos)) {
                m_position = videoPos;
                // Пропускаем несколько циклов для стабилизации
                return;
            }
        }

        // Если аудио сильно отстает от видео, ускоряем его
        if (avDiff < -150) {
            // Пропускаем несколько аудиофреймов для догона
            for (int i = 0; i < 2; ++i) {
                m_ffmpeg->getNextAudioData(1024);
            }
            return;
        }
        
        // Если аудио опережает видео более чем на 100 мс, подаем тишину
        if (avDiff > 100) {
            QByteArray silence(1024 * 2 * 2, 0); // 1024 сэмпла * 2 канала * 2 байта
            m_audioDevice->write(silence);
            return;
        }
    }
    
    // Получаем аудиоданные с улучшенной буферизацией
    QByteArray audioData;
    const int chunksToBuffer = 1; // Уменьшаем до 1 чанка для более стабильной работы
    
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
        static int emptyDataCount = 0;
        emptyDataCount++;
        if (emptyDataCount > 20) {
            qDebug() << "scheduleNextAudioChunk: too many empty data chunks, stopping audio";
            stop();
            emptyDataCount = 0;
        }
        return;
    }
    
    // Сбрасываем счетчик пустых данных
    static int emptyDataCount = 0;
    emptyDataCount = 0;
    
    // Если устройство закрыто, пытаемся его перезапустить
    if (!m_audioDevice->isOpen()) {
        m_audioDevice = m_audioSink->start();
        if (!m_audioDevice || !m_audioDevice->isOpen()) {
            if (m_audioTimer) {
                m_audioTimer->stop();
            }
            return;
        }
    }
    
    if (!m_audioDevice->isWritable()) {
        QThread::msleep(10);
        if (!m_audioDevice->isWritable()) {
            if (m_audioTimer) {
                m_audioTimer->stop();
            }
            return;
        }
    }
    
    // Записываем аудиоданные с гарантией полной записи
    qint64 totalWritten = 0;
    int maxAttempts = 10; // Максимальное количество попыток записи
    
    while (totalWritten < audioData.size() && maxAttempts > 0) {
        QByteArray remainingData = audioData.mid(totalWritten);
        qint64 written = m_audioDevice->write(remainingData);
        
        if (written < 0) {
            // Если произошла ошибка записи, пытаемся перезапустить устройство
            m_audioDevice = m_audioSink->start();
            if (m_audioDevice && m_audioDevice->isWritable()) {
                written = m_audioDevice->write(remainingData);
                if (written < 0) {
                    return;
                }
            } else {
                return;
            }
        } else if (written == 0) {
            // Если ничего не записали, ждем немного и пробуем снова
            QThread::msleep(5);
            maxAttempts--;
            continue;
        }
        
        totalWritten += written;
        
        // Если записали не все данные, ждем немного перед следующей попыткой
        if (totalWritten < audioData.size()) {
            QThread::msleep(2);
        }
    }
    
    // Логируем только если не удалось записать все данные
    if (totalWritten != audioData.size()) {
        qDebug() << "scheduleNextAudioChunk: incomplete write:" << totalWritten << "of" << audioData.size() << "bytes";
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
