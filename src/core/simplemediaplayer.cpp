#include "simplemediaplayer.h"
#include <QFileDialog>
#include <QStyle>
#include <QApplication>
#include <QDebug>
#include <QMimeData>

SimpleMediaPlayer::SimpleMediaPlayer(QWidget *parent)
    : QWidget(parent)
    , m_sliderPressed(false)
    , m_lastPosition(0)
{
    setAcceptDrops(true); // Включаем drag-and-drop
    // Создаем медиаплеер
    m_mediaPlayer = new QMediaPlayer(this);
    
    // Создаем аудиовыход
    m_audioOutput = new QAudioOutput(this);
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    
    // Создаем виджет для видео
    m_videoWidget = new QVideoWidget(this);
    m_mediaPlayer->setVideoOutput(m_videoWidget);
    
    // Создаем UI элементы
    m_playButton = new QPushButton(this);
    m_playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_playButton->setToolTip("Play/Pause");
    
    m_stopButton = new QPushButton(this);
    m_stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_stopButton->setToolTip("Stop");
    
    m_openButton = new QPushButton(this);
    m_openButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    m_openButton->setToolTip("Open File");
    
    m_resetButton = new QPushButton(this);
    m_resetButton->setIcon(style()->standardIcon(QStyle::SP_DialogResetButton));
    m_resetButton->setToolTip("Reset");
    
    m_positionSlider = new QSlider(Qt::Horizontal, this);
    m_positionSlider->setRange(0, 0);
    m_positionSlider->setToolTip("Position");
    
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(50);
    m_volumeSlider->setToolTip("Volume");
    
    m_timeLabel = new QLabel("00:00 / 00:00", this);
    
    m_infoLabel = new QLabel("Drag and drop a media file here or click 'Open File' button", this);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    
    // Настраиваем layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_videoWidget);
    mainLayout->addWidget(m_infoLabel);
    
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    controlsLayout->addWidget(m_playButton);
    controlsLayout->addWidget(m_stopButton);
    controlsLayout->addWidget(m_openButton);
    controlsLayout->addWidget(m_resetButton);
    controlsLayout->addWidget(m_positionSlider);
    controlsLayout->addWidget(m_timeLabel);
    controlsLayout->addWidget(new QLabel("Vol:"));
    controlsLayout->addWidget(m_volumeSlider);
    
    mainLayout->addLayout(controlsLayout);
    
    // Подключаем сигналы
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, &SimpleMediaPlayer::onPositionChanged);
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this, &SimpleMediaPlayer::onDurationChanged);
    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this, &SimpleMediaPlayer::onPlaybackStateChanged);
    
    connect(m_playButton, &QPushButton::clicked, [this]() {
        if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
            pause();
        } else {
            play();
        }
    });
    
    connect(m_stopButton, &QPushButton::clicked, this, &SimpleMediaPlayer::stop);
    
    connect(m_openButton, &QPushButton::clicked, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, 
            "Open Media File", 
            QDir::homePath(),
            "Media Files (*.mp4 *.avi *.mkv *.mov *.wmv *.flv *.webm *.mp3 *.wav *.flac *.m4a *.aac)");
        
        if (!filePath.isEmpty()) {
            openFile(filePath);
        }
    });
    
    connect(m_resetButton, &QPushButton::clicked, this, &SimpleMediaPlayer::reset);
    
    connect(m_positionSlider, &QSlider::sliderPressed, this, &SimpleMediaPlayer::onSliderPressed);
    connect(m_positionSlider, &QSlider::sliderReleased, this, &SimpleMediaPlayer::onSliderReleased);
    connect(m_positionSlider, &QSlider::sliderMoved, this, &SimpleMediaPlayer::onSliderMoved);
    
    connect(m_volumeSlider, &QSlider::valueChanged, m_audioOutput, &QAudioOutput::setVolume);
    
    // Устанавливаем размер окна
    resize(800, 600);
    setWindowTitle("Simple Media Player");
}

SimpleMediaPlayer::~SimpleMediaPlayer()
{
    qDebug() << "SimpleMediaPlayer::~SimpleMediaPlayer() called";
}

bool SimpleMediaPlayer::openFile(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return false;
    }
    
    m_mediaPlayer->setSource(QUrl::fromLocalFile(filePath));
    
    // Скрываем информационную метку при загрузке файла
    m_infoLabel->hide();
    
    qDebug() << "SimpleMediaPlayer: opened file:" << filePath;
    emit fileLoaded(filePath);
    
    return true;
}

void SimpleMediaPlayer::play()
{
    m_mediaPlayer->play();
}

void SimpleMediaPlayer::pause()
{
    m_mediaPlayer->pause();
}

void SimpleMediaPlayer::stop()
{
    m_mediaPlayer->stop();
}

void SimpleMediaPlayer::reset()
{
    m_mediaPlayer->stop();
    m_mediaPlayer->setSource(QUrl());
    m_infoLabel->show();  // Показываем информационную метку
    m_timeLabel->setText("00:00 / 00:00");
    m_positionSlider->setRange(0, 0);
}

void SimpleMediaPlayer::seek(qint64 position)
{
    m_mediaPlayer->setPosition(position);
}

qint64 SimpleMediaPlayer::position() const
{
    return m_mediaPlayer->position();
}

qint64 SimpleMediaPlayer::duration() const
{
    return m_mediaPlayer->duration();
}

bool SimpleMediaPlayer::isPlaying() const
{
    return m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState;
}

void SimpleMediaPlayer::onPositionChanged(qint64 position)
{
    if (!m_sliderPressed) {
        m_positionSlider->setValue(position);
    }
    
    // Обновляем время
    qint64 duration = m_mediaPlayer->duration();
    QString timeText = QString("%1:%2 / %3:%4")
        .arg(position / 60000, 2, 10, QChar('0'))
        .arg((position % 60000) / 1000, 2, 10, QChar('0'))
        .arg(duration / 60000, 2, 10, QChar('0'))
        .arg((duration % 60000) / 1000, 2, 10, QChar('0'));
    
    m_timeLabel->setText(timeText);
    
    emit positionChanged(position);
}

void SimpleMediaPlayer::onDurationChanged(qint64 duration)
{
    m_positionSlider->setRange(0, duration);
    emit durationChanged(duration);
}

void SimpleMediaPlayer::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    switch (state) {
        case QMediaPlayer::PlayingState:
            m_playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
            break;
        case QMediaPlayer::PausedState:
        case QMediaPlayer::StoppedState:
            m_playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
            break;
    }
    
    emit playbackStateChanged(state == QMediaPlayer::PlayingState);
}

void SimpleMediaPlayer::onSliderPressed()
{
    m_sliderPressed = true;
}

void SimpleMediaPlayer::onSliderReleased()
{
    m_sliderPressed = false;
    m_mediaPlayer->setPosition(m_positionSlider->value());
}

void SimpleMediaPlayer::onSliderMoved(int value)
{
    if (m_sliderPressed) {
        qint64 duration = m_mediaPlayer->duration();
        QString timeText = QString("%1:%2 / %3:%4")
            .arg(value / 60000, 2, 10, QChar('0'))
            .arg((value % 60000) / 1000, 2, 10, QChar('0'))
            .arg(duration / 60000, 2, 10, QChar('0'))
            .arg((duration % 60000) / 1000, 2, 10, QChar('0'));
        
        m_timeLabel->setText(timeText);
    }
}

void SimpleMediaPlayer::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            event->acceptProposedAction();
        }
    }
}

void SimpleMediaPlayer::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            QString filePath = urls.first().toLocalFile();
            openFile(filePath);
            play();
        }
    }
} 