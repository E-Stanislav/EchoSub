#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QByteArray>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

struct FFmpegContext {
    AVFormatContext *formatCtx;
    AVCodecContext *videoCodecCtx;
    AVCodecContext *audioCodecCtx;
    SwsContext *swsCtx;
    SwrContext *swrCtx;
    int videoStream;
    int audioStream;
    bool initialized;
};

class FFmpegWrapper : public QObject {
    Q_OBJECT

public:
    explicit FFmpegWrapper(QObject *parent = nullptr);
    ~FFmpegWrapper();

    // File operations
    bool openFile(const QString &filePath);
    void close();

    // Video operations
    bool hasVideo() const;
    int getVideoWidth() const;
    int getVideoHeight() const;
    qint64 getVideoDuration() const;
    double getVideoFrameRate() const;
    
    // Audio operations
    bool hasAudio() const;
    int getSampleRate() const;
    int getChannels() const;
    qint64 getAudioDuration() const;

    // Новый метод для задания выходного аудиоформата
    void setOutputAudioFormat(int sampleRate, int channels, AVSampleFormat sampleFmt);

    // Playback operations (new efficient methods)
    bool startPlayback(qint64 startMs = 0);
    QImage getNextFrame();
    QByteArray getNextAudioData(int maxSamples = 4096);
    bool seekToTime(qint64 timestampMs);
    qint64 getCurrentTime() const;
    
    // Legacy methods (for compatibility)
    QImage getVideoFrame(qint64 timestamp);
    bool isOpen() const { return m_isOpen; }

    // Metadata
    qint64 getDuration() const;
    QString getFormat() const;
    QString getCodec() const;

signals:
    void error(const QString &message);

private:
    bool initializeVideo();
    bool initializeAudio();
    void cleanup();
    void reinitializeAudioResampler();
    QImage convertFrameToQImage(AVFrame *frame);
    QByteArray convertAudioFrame(AVFrame *frame);
    
    // New private methods for efficient playback
    bool readNextPacket();
    bool decodeVideoPacket(AVPacket *packet);
    bool decodeAudioPacket(AVPacket *packet);
    void flushDecoders();

    std::unique_ptr<FFmpegContext> m_ctx;
    bool m_isOpen;
    
    // Playback state
    bool m_playbackStarted;
    qint64 m_currentTime;
    qint64 m_lastVideoPts;
    qint64 m_lastAudioPts;
    
    // Frame buffers
    AVFrame *m_videoFrame;
    AVFrame *m_audioFrame;
    AVPacket *m_packet;
    
    // Audio conversion
    uint8_t *m_audioBuffer;
    int m_audioBufferSize;

    // Выходной аудиоформат для ресемплинга
    int m_outSampleRate;
    int m_outChannels;
    AVSampleFormat m_outSampleFmt;

    QString m_filePath;
}; 