#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTimeEdit>
#include <QComboBox>
#include <QSpinBox>

class ControlsPanel : public QWidget {
    Q_OBJECT

public:
    explicit ControlsPanel(QWidget *parent = nullptr);
    ~ControlsPanel();

    void setDuration(qint64 duration);
    void setPosition(qint64 position);
    void setState(bool playing, bool paused);
    void enableControls(bool enable);

signals:
    void playClicked();
    void pauseClicked();
    void stopClicked();
    void seekRequested(qint64 position);
    void volumeChanged(int volume);
    void speedChanged(double speed);

private slots:
    void onPlayClicked();
    void onPauseClicked();
    void onStopClicked();
    void onSliderMoved(int value);
    void onSliderPressed();
    void onSliderReleased();
    void onVolumeChanged(int value);
    void onSpeedChanged(int index);

private:
    void setupUI();
    void updateTimeLabels();
    QString formatTime(qint64 ms) const;

    // Layout
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_controlsLayout;
    QHBoxLayout *m_timeLayout;
    QHBoxLayout *m_volumeLayout;

    // Playback controls
    QPushButton *m_playButton;
    QPushButton *m_pauseButton;
    QPushButton *m_stopButton;
    QPushButton *m_prevButton;
    QPushButton *m_nextButton;

    // Progress
    QSlider *m_progressSlider;
    QLabel *m_timeLabel;
    QLabel *m_durationLabel;

    // Volume and speed
    QSlider *m_volumeSlider;
    QLabel *m_volumeLabel;
    QComboBox *m_speedCombo;
    QLabel *m_speedLabel;

    // State
    qint64 m_duration;
    qint64 m_position;
    bool m_isPlaying;
    bool m_isPaused;
    bool m_sliderPressed;
}; 