#include "ui/controlspanel.h"
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStyle>

ControlsPanel::ControlsPanel(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_controlsLayout(nullptr)
    , m_timeLayout(nullptr)
    , m_volumeLayout(nullptr)
    , m_playButton(nullptr)
    , m_pauseButton(nullptr)
    , m_stopButton(nullptr)
    , m_prevButton(nullptr)
    , m_nextButton(nullptr)
    , m_progressSlider(nullptr)
    , m_timeLabel(nullptr)
    , m_durationLabel(nullptr)
    , m_volumeSlider(nullptr)
    , m_volumeLabel(nullptr)
    , m_speedCombo(nullptr)
    , m_speedLabel(nullptr)
    , m_duration(0)
    , m_position(0)
    , m_isPlaying(false)
    , m_isPaused(false)
    , m_sliderPressed(false)
{
    setupUI();
    enableControls(false);
}

ControlsPanel::~ControlsPanel()
{
}

void ControlsPanel::setDuration(qint64 duration)
{
    m_duration = duration;
    if (m_progressSlider) {
        m_progressSlider->setMaximum(static_cast<int>(duration));
    }
    updateTimeLabels();
}

void ControlsPanel::setPosition(qint64 position)
{
    m_position = position;
    if (m_progressSlider && !m_sliderPressed) {
        m_progressSlider->setValue(static_cast<int>(position));
    }
    updateTimeLabels();
}

void ControlsPanel::setState(bool playing, bool paused)
{
    m_isPlaying = playing;
    m_isPaused = paused;
    
    if (m_playButton) {
        m_playButton->setEnabled(!playing || paused);
    }
    if (m_pauseButton) {
        m_pauseButton->setEnabled(playing && !paused);
    }
    if (m_stopButton) {
        m_stopButton->setEnabled(playing || paused);
    }
}

void ControlsPanel::enableControls(bool enable)
{
    if (m_playButton) m_playButton->setEnabled(enable);
    if (m_pauseButton) m_pauseButton->setEnabled(enable);
    if (m_stopButton) m_stopButton->setEnabled(enable);
    if (m_prevButton) m_prevButton->setEnabled(enable);
    if (m_nextButton) m_nextButton->setEnabled(enable);
    if (m_progressSlider) m_progressSlider->setEnabled(enable);
    if (m_volumeSlider) m_volumeSlider->setEnabled(enable);
    if (m_speedCombo) m_speedCombo->setEnabled(enable);
}

void ControlsPanel::onPlayClicked()
{
    emit playClicked();
}

void ControlsPanel::onPauseClicked()
{
    emit pauseClicked();
}

void ControlsPanel::onStopClicked()
{
    emit stopClicked();
}

void ControlsPanel::onSliderMoved(int value)
{
    if (m_sliderPressed) {
        emit seekRequested(static_cast<qint64>(value));
    }
}

void ControlsPanel::onSliderPressed()
{
    m_sliderPressed = true;
}

void ControlsPanel::onSliderReleased()
{
    m_sliderPressed = false;
    if (m_progressSlider) {
        emit seekRequested(static_cast<qint64>(m_progressSlider->value()));
    }
}

void ControlsPanel::onVolumeChanged(int value)
{
    emit volumeChanged(value);
    if (m_volumeLabel) {
        m_volumeLabel->setText(QString("%1%").arg(value));
    }
}

void ControlsPanel::onSpeedChanged(int index)
{
    double speed = 1.0;
    switch (index) {
    case 0: speed = 0.25; break;
    case 1: speed = 0.5; break;
    case 2: speed = 0.75; break;
    case 3: speed = 1.0; break;
    case 4: speed = 1.25; break;
    case 5: speed = 1.5; break;
    case 6: speed = 2.0; break;
    }
    emit speedChanged(speed);
}

void ControlsPanel::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(5);
    m_mainLayout->setContentsMargins(10, 5, 10, 5);
    
    // Progress slider and time labels
    m_timeLayout = new QHBoxLayout();
    m_progressSlider = new QSlider(Qt::Horizontal);
    m_progressSlider->setMinimum(0);
    m_progressSlider->setMaximum(100);
    m_progressSlider->setValue(0);
    
    m_timeLabel = new QLabel("00:00");
    m_durationLabel = new QLabel("00:00");
    
    m_timeLayout->addWidget(m_timeLabel);
    m_timeLayout->addWidget(m_progressSlider, 1);
    m_timeLayout->addWidget(m_durationLabel);
    
    // Control buttons
    m_controlsLayout = new QHBoxLayout();
    
    m_prevButton = new QPushButton();
    m_prevButton->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
    m_prevButton->setToolTip("Previous");
    
    m_playButton = new QPushButton();
    m_playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_playButton->setToolTip("Play");
    
    m_pauseButton = new QPushButton();
    m_pauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    m_pauseButton->setToolTip("Pause");
    
    m_stopButton = new QPushButton();
    m_stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_stopButton->setToolTip("Stop");
    
    m_nextButton = new QPushButton();
    m_nextButton->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
    m_nextButton->setToolTip("Next");
    
    m_controlsLayout->addStretch();
    m_controlsLayout->addWidget(m_prevButton);
    m_controlsLayout->addWidget(m_playButton);
    m_controlsLayout->addWidget(m_pauseButton);
    m_controlsLayout->addWidget(m_stopButton);
    m_controlsLayout->addWidget(m_nextButton);
    m_controlsLayout->addStretch();
    
    // Volume and speed controls
    m_volumeLayout = new QHBoxLayout();
    
    m_volumeLabel = new QLabel("100%");
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setMinimum(0);
    m_volumeSlider->setMaximum(100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setMaximumWidth(100);
    
    m_speedLabel = new QLabel("Speed:");
    m_speedCombo = new QComboBox();
    m_speedCombo->addItems({"0.25x", "0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x"});
    m_speedCombo->setCurrentIndex(3); // 1.0x
    
    m_volumeLayout->addStretch();
    m_volumeLayout->addWidget(m_volumeLabel);
    m_volumeLayout->addWidget(m_volumeSlider);
    m_volumeLayout->addWidget(m_speedLabel);
    m_volumeLayout->addWidget(m_speedCombo);
    
    // Add all layouts to main layout
    m_mainLayout->addLayout(m_timeLayout);
    m_mainLayout->addLayout(m_controlsLayout);
    m_mainLayout->addLayout(m_volumeLayout);
    
    // Connect signals
    connect(m_playButton, &QPushButton::clicked, this, &ControlsPanel::onPlayClicked);
    connect(m_pauseButton, &QPushButton::clicked, this, &ControlsPanel::onPauseClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &ControlsPanel::onStopClicked);
    
    connect(m_progressSlider, &QSlider::sliderMoved, this, &ControlsPanel::onSliderMoved);
    connect(m_progressSlider, &QSlider::sliderPressed, this, &ControlsPanel::onSliderPressed);
    connect(m_progressSlider, &QSlider::sliderReleased, this, &ControlsPanel::onSliderReleased);
    
    connect(m_volumeSlider, &QSlider::valueChanged, this, &ControlsPanel::onVolumeChanged);
    connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ControlsPanel::onSpeedChanged);
}

void ControlsPanel::updateTimeLabels()
{
    if (m_timeLabel) {
        m_timeLabel->setText(formatTime(m_position));
    }
    if (m_durationLabel) {
        m_durationLabel->setText(formatTime(m_duration));
    }
}

QString ControlsPanel::formatTime(qint64 ms) const
{
    qint64 seconds = ms / 1000;
    qint64 minutes = seconds / 60;
    seconds = seconds % 60;
    qint64 hours = minutes / 60;
    minutes = minutes % 60;
    
    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
}
