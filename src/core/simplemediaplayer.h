#ifndef SIMPLEMEDIAPLAYER_H
#define SIMPLEMEDIAPLAYER_H

#include <QWidget>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QTimer>
#include <QMap>
#include "ui/videowidget.h"

class WhisperModelSettingsDialog;

class SimpleMediaPlayer : public QWidget
{
    Q_OBJECT

public:
    explicit SimpleMediaPlayer(QWidget *parent = nullptr);
    ~SimpleMediaPlayer();

    bool openFile(const QString &filePath);
    void play();
    void pause();
    void stop();
    void seek(qint64 position);
    void reset();
    qint64 position() const;
    qint64 duration() const;
    bool isPlaying() const;
    void createSubtitles();
    void createSubtitlesOverlay();

signals:
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void playbackStateChanged(bool playing);
    void fileLoaded(const QString &filePath);

private slots:
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onSliderPressed();
    void onSliderReleased();
    void onSliderMoved(int value);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QMediaPlayer *m_mediaPlayer;
    VideoGraphicsView *m_videoWidget;
    QAudioOutput *m_audioOutput;
    
    // UI elements
    QPushButton *m_playButton;
    QPushButton *m_stopButton;
    QPushButton *m_openButton;
    QPushButton *m_resetButton;
    QPushButton *m_settingsButton;
    QPushButton *m_subtitlesButton;
    QPushButton *m_subtitlesOverlayButton;
    QSlider *m_positionSlider;
    QSlider *m_volumeSlider;
    QLabel *m_timeLabel;
    QLabel *m_infoLabel;
    
    bool m_sliderPressed;
    qint64 m_lastPosition;
    
    // Методы для работы с субтитрами
    QMap<qint64, QString> parseSrtData(const QByteArray &srtData);
    void displaySubtitles(const QMap<qint64, QString> &subtitles);
};

#endif // SIMPLEMEDIAPLAYER_H 