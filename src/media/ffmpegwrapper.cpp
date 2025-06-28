#include "media/ffmpegwrapper.h"
#include <QDebug>
#include <QFileInfo>
#include <QPainter>
#include <QFont>

FFmpegWrapper::FFmpegWrapper(QObject *parent)
    : QObject(parent)
    , m_ctx(std::make_unique<FFmpegContext>())
    , m_isOpen(false)
    , m_playbackStarted(false)
    , m_currentTime(0)
    , m_lastVideoPts(0)
    , m_lastAudioPts(0)
    , m_videoFrame(nullptr)
    , m_audioFrame(nullptr)
    , m_packet(nullptr)
    , m_audioBuffer(nullptr)
    , m_audioBufferSize(0)
    , m_outSampleRate(44100)
    , m_outChannels(2)
    , m_outSampleFmt(AV_SAMPLE_FMT_S16)
{
    // Initialize FFmpeg
    avformat_network_init();
    
    // Initialize context
    m_ctx->formatCtx = nullptr;
    m_ctx->videoCodecCtx = nullptr;
    m_ctx->audioCodecCtx = nullptr;
    m_ctx->swsCtx = nullptr;
    m_ctx->swrCtx = nullptr;
    m_ctx->videoStream = -1;
    m_ctx->audioStream = -1;
    m_ctx->initialized = false;
    
    // Allocate frame buffers
    m_videoFrame = av_frame_alloc();
    m_audioFrame = av_frame_alloc();
    m_packet = av_packet_alloc();
    
    if (!m_videoFrame || !m_audioFrame || !m_packet) {
        qDebug() << "Failed to allocate frame buffers";
    }
}

FFmpegWrapper::~FFmpegWrapper()
{
    qDebug() << "FFmpegWrapper::~FFmpegWrapper() called";
    close();
    if (m_videoFrame) {
        qDebug() << "~FFmpegWrapper: av_frame_free videoFrame";
        av_frame_free(&m_videoFrame);
        qDebug() << "~FFmpegWrapper: videoFrame set to nullptr";
    }
    if (m_audioFrame) {
        qDebug() << "~FFmpegWrapper: av_frame_free audioFrame";
        av_frame_free(&m_audioFrame);
        qDebug() << "~FFmpegWrapper: audioFrame set to nullptr";
    }
    if (m_packet) {
        qDebug() << "~FFmpegWrapper: av_packet_free packet";
        av_packet_free(&m_packet);
        qDebug() << "~FFmpegWrapper: packet set to nullptr";
    }
    if (m_audioBuffer) {
        qDebug() << "~FFmpegWrapper: av_freep audioBuffer";
        av_freep(&m_audioBuffer);
        m_audioBuffer = nullptr;
        qDebug() << "~FFmpegWrapper: audioBuffer set to nullptr";
    }
}

bool FFmpegWrapper::openFile(const QString &filePath)
{
    close();
    
    m_filePath = filePath;
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        emit error("File does not exist");
        return false;
    }
    
    // Open input file
    if (avformat_open_input(&m_ctx->formatCtx, filePath.toUtf8().constData(), nullptr, nullptr) < 0) {
        emit error("Could not open input file");
        return false;
    }
    
    // Find stream info
    if (avformat_find_stream_info(m_ctx->formatCtx, nullptr) < 0) {
        emit error("Could not find stream information");
        return false;
    }
    
    // Find video and audio streams
    for (unsigned int i = 0; i < m_ctx->formatCtx->nb_streams; i++) {
        AVStream *stream = m_ctx->formatCtx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && m_ctx->videoStream < 0) {
            m_ctx->videoStream = i;
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && m_ctx->audioStream < 0) {
            m_ctx->audioStream = i;
        }
    }
    
    // Initialize codecs
    if (m_ctx->videoStream >= 0 && !initializeVideo()) {
        emit error("Failed to initialize video codec");
        return false;
    }
    
    if (m_ctx->audioStream >= 0 && !initializeAudio()) {
        emit error("Failed to initialize audio codec");
        return false;
    }
    
    m_isOpen = true;
    m_ctx->initialized = true;
    
    qDebug() << "Opened file:" << filePath;
    qDebug() << "Video stream:" << m_ctx->videoStream;
    qDebug() << "Audio stream:" << m_ctx->audioStream;
    
    return true;
}

void FFmpegWrapper::close()
{
    if (m_playbackStarted) {
        flushDecoders();
    }
    
    cleanup();
    m_isOpen = false;
    m_playbackStarted = false;
    m_currentTime = 0;
    m_lastVideoPts = 0;
    m_lastAudioPts = 0;
    m_filePath.clear();
}

bool FFmpegWrapper::startPlayback(qint64 startMs)
{
    if (!m_isOpen) {
        return false;
    }
    
    // Seek to start position
    if (startMs > 0 && !seekToTime(startMs)) {
        qDebug() << "Failed to seek to start position";
        return false;
    }
    
    // Flush decoders
    flushDecoders();
    
    m_playbackStarted = true;
    m_currentTime = startMs;
    m_lastVideoPts = 0;
    m_lastAudioPts = 0;
    
    qDebug() << "Started playback at" << startMs << "ms";
    return true;
}

QImage FFmpegWrapper::getNextFrame()
{
    if (!m_playbackStarted || !m_isOpen || m_ctx->videoStream < 0) {
        return QImage();
    }
    
    // Read packets until we get a video frame
    while (true) {
        if (!readNextPacket()) {
            return QImage(); // End of stream
        }
        
        if (m_packet->stream_index == m_ctx->videoStream) {
            if (decodeVideoPacket(m_packet)) {
                // Convert frame to QImage
                QImage image = convertFrameToQImage(m_videoFrame);
                
                // Update current time
                if (m_videoFrame->pts != AV_NOPTS_VALUE) {
                    AVStream *stream = m_ctx->formatCtx->streams[m_ctx->videoStream];
                    m_currentTime = av_rescale_q(m_videoFrame->pts, stream->time_base, AVRational{1, 1000});
                }
                
                return image;
            }
        }
    }
}

QByteArray FFmpegWrapper::getNextAudioData(int maxSamples)
{
    if (!m_playbackStarted || !m_isOpen || m_ctx->audioStream < 0) {
        return QByteArray();
    }
    
    // Read packets until we get audio data
    int attempts = 0;
    const int maxAttempts = 8; // Уменьшаем количество попыток для более быстрого ответа
    
    while (attempts < maxAttempts) {
        if (!readNextPacket()) {
            return QByteArray(); // End of stream
        }
        
        if (m_packet->stream_index == m_ctx->audioStream) {
            if (decodeAudioPacket(m_packet)) {
                QByteArray result = convertAudioFrame(m_audioFrame);
                if (!result.isEmpty()) {
                    // Обновляем текущее время на основе аудио PTS с минимальной задержкой
                    if (m_audioFrame->pts != AV_NOPTS_VALUE) {
                        AVStream *stream = m_ctx->formatCtx->streams[m_ctx->audioStream];
                        qint64 audioTime = av_rescale_q(m_audioFrame->pts, stream->time_base, AVRational{1, 1000});
                        // Уменьшаем задержку для лучшей синхронизации, но не обновляем слишком часто
                        if (audioTime > m_currentTime + 10) { // Обновляем только если разница больше 10ms
                            m_currentTime = audioTime - 10; // Задержка только 10ms
                            if (m_currentTime < 0) m_currentTime = 0;
                        }
                    }
                    return result;
                }
            }
        }
        
        attempts++;
    }
    
    // Если не удалось получить данные, возвращаем тишину для предотвращения прерываний
    int silenceBytes = maxSamples * m_outChannels * av_get_bytes_per_sample(m_outSampleFmt);
    return QByteArray(silenceBytes, 0);
}

bool FFmpegWrapper::seekToTime(qint64 timestampMs)
{
    if (!m_isOpen || !m_ctx->formatCtx) {
        return false;
    }
    
    // Seek to the target time
    int64_t seek_target = av_rescale_q(timestampMs, AVRational{1, 1000}, AVRational{1, AV_TIME_BASE});
    
    if (av_seek_frame(m_ctx->formatCtx, -1, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
        qDebug() << "Failed to seek to time" << timestampMs << "ms";
        return false;
    }
    
    // Flush decoders after seek
    flushDecoders();
    
    m_currentTime = timestampMs;
    qDebug() << "Seeked to" << timestampMs << "ms";
    return true;
}

qint64 FFmpegWrapper::getCurrentTime() const
{
    return m_currentTime;
}

bool FFmpegWrapper::readNextPacket()
{
    if (!m_isOpen || !m_ctx->formatCtx) {
        return false;
    }
    
    av_packet_unref(m_packet);
    int ret = av_read_frame(m_ctx->formatCtx, m_packet);
    
    if (ret < 0) {
        if (ret == AVERROR_EOF) {
            qDebug() << "End of stream reached";
        } else {
            qDebug() << "Error reading packet:" << ret;
        }
        return false;
    }
    
    return true;
}

bool FFmpegWrapper::decodeVideoPacket(AVPacket *packet)
{
    if (!m_ctx->videoCodecCtx || !m_videoFrame) {
        return false;
    }
    
    int ret = avcodec_send_packet(m_ctx->videoCodecCtx, packet);
    if (ret < 0) {
        qDebug() << "Error sending video packet to decoder";
        return false;
    }
    
    ret = avcodec_receive_frame(m_ctx->videoCodecCtx, m_videoFrame);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return false; // Need more packets or end of stream
        }
        qDebug() << "Error receiving video frame from decoder";
        return false;
    }
    
    return true;
}

bool FFmpegWrapper::decodeAudioPacket(AVPacket *packet)
{
    if (!m_ctx->audioCodecCtx || !m_audioFrame) {
        return false;
    }
    
    int ret = avcodec_send_packet(m_ctx->audioCodecCtx, packet);
    if (ret < 0) {
        qDebug() << "Error sending audio packet to decoder";
        return false;
    }
    
    ret = avcodec_receive_frame(m_ctx->audioCodecCtx, m_audioFrame);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return false; // Need more packets or end of stream
        }
        qDebug() << "Error receiving audio frame from decoder";
        return false;
    }
    
    return true;
}

void FFmpegWrapper::flushDecoders()
{
    if (m_ctx->videoCodecCtx) {
        avcodec_flush_buffers(m_ctx->videoCodecCtx);
    }
    if (m_ctx->audioCodecCtx) {
        avcodec_flush_buffers(m_ctx->audioCodecCtx);
    }
}

// Legacy method for compatibility
QImage FFmpegWrapper::getVideoFrame(qint64 timestamp)
{
    if (!m_isOpen || m_ctx->videoStream < 0) {
        return QImage();
    }
    
    // For legacy compatibility, do a seek and return next frame
    if (!seekToTime(timestamp)) {
        return QImage();
    }
    
    return getNextFrame();
}

// Video info methods
bool FFmpegWrapper::hasVideo() const
{
    return m_isOpen && m_ctx->videoStream >= 0;
}

int FFmpegWrapper::getVideoWidth() const
{
    if (m_ctx->videoCodecCtx) {
        return m_ctx->videoCodecCtx->width;
    }
    return 0;
}

int FFmpegWrapper::getVideoHeight() const
{
    if (m_ctx->videoCodecCtx) {
        return m_ctx->videoCodecCtx->height;
    }
    return 0;
}

qint64 FFmpegWrapper::getVideoDuration() const
{
    if (m_ctx->formatCtx && m_ctx->videoStream >= 0) {
        AVStream *stream = m_ctx->formatCtx->streams[m_ctx->videoStream];
        if (stream->duration != AV_NOPTS_VALUE) {
            return av_rescale_q(stream->duration, stream->time_base, AVRational{1, 1000});
        }
    }
    return 0;
}

double FFmpegWrapper::getVideoFrameRate() const
{
    if (m_ctx->formatCtx && m_ctx->videoStream >= 0) {
        AVStream *stream = m_ctx->formatCtx->streams[m_ctx->videoStream];
        if (stream->avg_frame_rate.num && stream->avg_frame_rate.den) {
            return av_q2d(stream->avg_frame_rate);
        }
    }
    return 0.0;
}

// Audio info methods
bool FFmpegWrapper::hasAudio() const
{
    return m_isOpen && m_ctx->audioStream >= 0;
}

int FFmpegWrapper::getSampleRate() const
{
    if (m_ctx->audioCodecCtx) {
        return m_ctx->audioCodecCtx->sample_rate;
    }
    return 0;
}

int FFmpegWrapper::getChannels() const
{
#if LIBAVCODEC_VERSION_MAJOR >= 60
    if (m_ctx->audioCodecCtx) {
        return m_ctx->audioCodecCtx->ch_layout.nb_channels;
    } else if (m_ctx->audioStream >= 0 && m_ctx->formatCtx) {
        AVStream *stream = m_ctx->formatCtx->streams[m_ctx->audioStream];
        if (stream && stream->codecpar) {
            return stream->codecpar->ch_layout.nb_channels;
        }
    }
    return 2;
#else
    if (m_ctx->audioCodecCtx) {
        return m_ctx->audioCodecCtx->channels;
    } else if (m_ctx->audioStream >= 0 && m_ctx->formatCtx) {
        AVStream *stream = m_ctx->formatCtx->streams[m_ctx->audioStream];
        if (stream && stream->codecpar)
            return stream->codecpar->channels;
    }
    return 2;
#endif
}

qint64 FFmpegWrapper::getAudioDuration() const
{
    if (m_ctx->formatCtx && m_ctx->audioStream >= 0) {
        AVStream *stream = m_ctx->formatCtx->streams[m_ctx->audioStream];
        if (stream->duration != AV_NOPTS_VALUE) {
            return av_rescale_q(stream->duration, stream->time_base, AVRational{1, 1000});
        }
    }
    return 0;
}

void FFmpegWrapper::cleanup()
{
    qDebug() << "FFmpegWrapper::cleanup() called";
    if (m_ctx->swsCtx) {
        qDebug() << "cleanup: sws_freeContext";
        sws_freeContext(m_ctx->swsCtx);
        m_ctx->swsCtx = nullptr;
        qDebug() << "cleanup: swsCtx set to nullptr";
    }
    if (m_ctx->swrCtx) {
        qDebug() << "cleanup: swr_free";
        swr_free(&m_ctx->swrCtx);
        qDebug() << "cleanup: swrCtx set to nullptr";
    }
    if (m_ctx->videoCodecCtx) {
        qDebug() << "cleanup: avcodec_free_context (video)";
        avcodec_free_context(&m_ctx->videoCodecCtx);
        qDebug() << "cleanup: videoCodecCtx set to nullptr";
    }
    if (m_ctx->audioCodecCtx) {
        qDebug() << "cleanup: avcodec_free_context (audio)";
        avcodec_free_context(&m_ctx->audioCodecCtx);
        qDebug() << "cleanup: audioCodecCtx set to nullptr";
    }
    if (m_ctx->formatCtx) {
        qDebug() << "cleanup: avformat_close_input";
        avformat_close_input(&m_ctx->formatCtx);
        qDebug() << "cleanup: formatCtx set to nullptr";
    }
    if (m_audioBuffer) {
        qDebug() << "cleanup: av_freep audioBuffer";
        av_freep(&m_audioBuffer);
        m_audioBuffer = nullptr;
        qDebug() << "cleanup: audioBuffer set to nullptr";
    }
    m_ctx->videoStream = -1;
    m_ctx->audioStream = -1;
    m_ctx->initialized = false;
}

bool FFmpegWrapper::initializeVideo()
{
    if (m_ctx->videoStream < 0) {
        return false;
    }
    
    AVStream *stream = m_ctx->formatCtx->streams[m_ctx->videoStream];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    
    if (!codec) {
        return false;
    }
    
    m_ctx->videoCodecCtx = avcodec_alloc_context3(codec);
    if (!m_ctx->videoCodecCtx) {
        return false;
    }
    
    if (avcodec_parameters_to_context(m_ctx->videoCodecCtx, stream->codecpar) < 0) {
        return false;
    }
    
    if (avcodec_open2(m_ctx->videoCodecCtx, codec, nullptr) < 0) {
        return false;
    }
    
    // Initialize SwsContext for frame conversion
    int width = m_ctx->videoCodecCtx->width;
    int height = m_ctx->videoCodecCtx->height;
    
    m_ctx->swsCtx = sws_getContext(
        width, height, m_ctx->videoCodecCtx->pix_fmt,
        width, height, AV_PIX_FMT_RGB32,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!m_ctx->swsCtx) {
        qDebug() << "Failed to create SwsContext";
        return false;
    }
    
    qDebug() << "Video initialized:" << width << "x" << height;
    return true;
}

bool FFmpegWrapper::initializeAudio()
{
    if (m_ctx->audioStream < 0) {
        return false;
    }
    
    AVStream *stream = m_ctx->formatCtx->streams[m_ctx->audioStream];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    
    if (!codec) {
        return false;
    }
    
    m_ctx->audioCodecCtx = avcodec_alloc_context3(codec);
    if (!m_ctx->audioCodecCtx) {
        return false;
    }
    
    if (avcodec_parameters_to_context(m_ctx->audioCodecCtx, stream->codecpar) < 0) {
        return false;
    }
    
    if (avcodec_open2(m_ctx->audioCodecCtx, codec, nullptr) < 0) {
        return false;
    }
    
    // Initialize SwrContext for audio conversion
    int channels = getChannels();
#if LIBAVCODEC_VERSION_MAJOR >= 60
    SwrContext *ctx = swr_alloc();
    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, m_outChannels);
    if (swr_alloc_set_opts2(&ctx,
        &out_ch_layout, m_outSampleFmt, m_outSampleRate,
        &m_ctx->audioCodecCtx->ch_layout, m_ctx->audioCodecCtx->sample_fmt, m_ctx->audioCodecCtx->sample_rate,
        0, nullptr) < 0) {
        qDebug() << "Failed to set SwrContext opts";
        return false;
    }
    m_ctx->swrCtx = ctx;
#else
    m_ctx->swrCtx = swr_alloc_set_opts(nullptr,
        av_get_default_channel_layout(m_outChannels), m_outSampleFmt, m_outSampleRate,
        av_get_default_channel_layout(channels),
        m_ctx->audioCodecCtx->sample_fmt,
        m_ctx->audioCodecCtx->sample_rate,
        0, nullptr
    );
#endif
    
    if (!m_ctx->swrCtx) {
        qDebug() << "Failed to create SwrContext";
        return false;
    }
    
    if (swr_init(m_ctx->swrCtx) < 0) {
        qDebug() << "Failed to initialize SwrContext";
        return false;
    }
    
    // Allocate audio buffer
    m_audioBufferSize = av_samples_get_buffer_size(nullptr, m_outChannels, 4096, m_outSampleFmt, 0);
    m_audioBuffer = (uint8_t*)av_malloc(m_audioBufferSize);
    
    if (!m_audioBuffer) {
        qDebug() << "Failed to allocate audio buffer";
        return false;
    }
    
    qDebug() << "Audio initialized: sample rate" << m_ctx->audioCodecCtx->sample_rate 
             << "channels" << channels;
    return true;
}

QImage FFmpegWrapper::convertFrameToQImage(AVFrame *frame)
{
    if (!frame || !m_ctx->videoCodecCtx) {
        return QImage();
    }

    // Get video dimensions
    int width = m_ctx->videoCodecCtx->width;
    int height = m_ctx->videoCodecCtx->height;
    
    if (width <= 0 || height <= 0) {
        return QImage();
    }

    // Create QImage with RGB32 format
    QImage image(width, height, QImage::Format_RGB32);
    
    // SwsContext should be already initialized in initializeVideo()
    if (!m_ctx->swsCtx) {
        qDebug() << "SwsContext not initialized, creating it now";
        m_ctx->swsCtx = sws_getContext(
            width, height, m_ctx->videoCodecCtx->pix_fmt,
            width, height, AV_PIX_FMT_RGB32,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        
        if (!m_ctx->swsCtx) {
            qDebug() << "Failed to create SwsContext";
            return QImage();
        }
    }

    // Setup destination frame
    uint8_t *destData[4] = {image.bits(), nullptr, nullptr, nullptr};
    int destLinesize[4] = {static_cast<int>(image.bytesPerLine()), 0, 0, 0};

    // Convert frame
    int result = sws_scale(m_ctx->swsCtx,
                          frame->data, frame->linesize, 0, height,
                          destData, destLinesize);
    
    if (result < 0) {
        qDebug() << "Failed to convert frame to RGB";
        return QImage();
    }

    return image;
}

QByteArray FFmpegWrapper::convertAudioFrame(AVFrame *frame)
{
    if (!frame || !m_ctx->swrCtx || !m_audioBuffer) {
        return QByteArray();
    }
    
    // Вычисляем максимальное количество выходных сэмплов
    int maxOutSamples = av_rescale_rnd(frame->nb_samples, m_outSampleRate, 
                                      m_ctx->audioCodecCtx->sample_rate, AV_ROUND_UP);
    
    // Убеждаемся, что буфер достаточно большой
    int requiredBufferSize = av_samples_get_buffer_size(nullptr, m_outChannels, 
                                                       maxOutSamples, m_outSampleFmt, 0);
    if (requiredBufferSize > m_audioBufferSize) {
        av_freep(&m_audioBuffer);
        m_audioBuffer = (uint8_t*)av_malloc(requiredBufferSize);
        m_audioBufferSize = requiredBufferSize;
        if (!m_audioBuffer) {
            return QByteArray();
        }
    }
    
    // Convert audio frame to target format
    int outSamples = swr_convert(m_ctx->swrCtx, &m_audioBuffer, maxOutSamples,
                                (const uint8_t**)frame->data, frame->nb_samples);
    
    if (outSamples < 0) {
        return QByteArray();
    }
    
    int outBytes = outSamples * m_outChannels * av_get_bytes_per_sample(m_outSampleFmt);
    
    // Создаем копию данных, чтобы избежать проблем с dangling pointer
    return QByteArray((const char*)m_audioBuffer, outBytes);
}

void FFmpegWrapper::setOutputAudioFormat(int sampleRate, int channels, AVSampleFormat sampleFmt) {
    m_outSampleRate = sampleRate;
    m_outChannels = channels;
    m_outSampleFmt = sampleFmt;
    
    // Переинициализируем ресемплер если аудио уже инициализировано
    if (m_ctx->audioCodecCtx && m_ctx->swrCtx) {
        reinitializeAudioResampler();
    }
}

void FFmpegWrapper::reinitializeAudioResampler() {
    if (m_ctx->swrCtx) {
        swr_free(&m_ctx->swrCtx);
    }
    if (!m_ctx->audioCodecCtx) {
        return;
    }
    int channels = getChannels();
#if LIBAVCODEC_VERSION_MAJOR >= 60
    SwrContext *ctx = swr_alloc();
    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, m_outChannels);
    if (swr_alloc_set_opts2(&ctx,
        &out_ch_layout, m_outSampleFmt, m_outSampleRate,
        &m_ctx->audioCodecCtx->ch_layout, m_ctx->audioCodecCtx->sample_fmt, m_ctx->audioCodecCtx->sample_rate,
        0, nullptr) < 0) {
        qDebug() << "Failed to set SwrContext opts";
        return;
    }
    m_ctx->swrCtx = ctx;
#else
    m_ctx->swrCtx = swr_alloc_set_opts(nullptr,
        av_get_default_channel_layout(m_outChannels), m_outSampleFmt, m_outSampleRate,
        av_get_default_channel_layout(channels),
        m_ctx->audioCodecCtx->sample_fmt,
        m_ctx->audioCodecCtx->sample_rate,
        0, nullptr
    );
#endif
    if (!m_ctx->swrCtx) {
        qDebug() << "Failed to create SwrContext";
        return;
    }
    if (swr_init(m_ctx->swrCtx) < 0) {
        qDebug() << "Failed to initialize SwrContext";
        return;
    }
    if (m_audioBuffer) {
        av_freep(&m_audioBuffer);
        m_audioBuffer = nullptr;
    }
    m_audioBufferSize = av_samples_get_buffer_size(nullptr, m_outChannels, 4096, m_outSampleFmt, 0);
    m_audioBuffer = (uint8_t*)av_malloc(m_audioBufferSize);
    if (!m_audioBuffer) {
        qDebug() << "Failed to allocate audio buffer";
        return;
    }
    qDebug() << "Audio resampler reinitialized: sample rate" << m_outSampleRate 
             << "channels" << m_outChannels << "format" << m_outSampleFmt;
}
