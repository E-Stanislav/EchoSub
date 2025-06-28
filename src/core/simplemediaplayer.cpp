#include "simplemediaplayer.h"
#include "WhisperModelSettingsDialog.h"
#include <QFileDialog>
#include <QStyle>
#include <QApplication>
#include <QDebug>
#include <QMimeData>
#include <QProcess>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDir>
#include <QSettings>

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
    m_videoWidget = new DraggableVideoWidget(this);
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
    
    m_fullscreenButton = new QPushButton(this);
    m_fullscreenButton->setText("⛶");
    m_fullscreenButton->setToolTip("Во весь экран");
    
    m_settingsButton = new QPushButton(this);
    m_settingsButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_settingsButton->setToolTip("Настройки");
    
    m_subtitlesButton = new QPushButton(this);
    m_subtitlesButton->setText("🎤");
    m_subtitlesButton->setToolTip("Создать субтитры (Whisper)");
    
    m_positionSlider = new QSlider(Qt::Horizontal, this);
    m_positionSlider->setMinimum(0);
    m_positionSlider->setMaximum(0);
    m_positionSlider->setSingleStep(1);
    m_positionSlider->setPageStep(10);
    m_positionSlider->setTracking(true);
    m_positionSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); // Растягиваем по горизонтали
    
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedWidth(100); // Делаем слайдер громкости уже
    
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
    controlsLayout->addWidget(m_fullscreenButton);
    controlsLayout->addWidget(m_settingsButton);
    controlsLayout->addWidget(m_subtitlesButton);
    
    controlsLayout->addWidget(m_positionSlider, /*stretch=*/2); // Слайдер перемотки занимает больше места
    controlsLayout->addWidget(m_timeLabel);
    controlsLayout->addWidget(new QLabel("Vol:", this));
    controlsLayout->addWidget(m_volumeSlider, /*stretch=*/0); // Слайдер громкости не растягивается
    
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
    
    // Подключаем сигнал drop файла от DraggableVideoWidget
    connect(m_videoWidget, &DraggableVideoWidget::fileDropped, [this](const QString &filePath) {
        qDebug() << "SimpleMediaPlayer: file dropped on video widget:" << filePath;
        if (openFile(filePath)) {
            play();
        }
    });
    
    connect(m_fullscreenButton, &QPushButton::clicked, [this]() {
        if (isFullScreen()) {
            showNormal();
            m_fullscreenButton->setText("⛶");
            m_fullscreenButton->setToolTip("Во весь экран");
        } else {
            showFullScreen();
            m_fullscreenButton->setText("❐");
            m_fullscreenButton->setToolTip("Выйти из полноэкранного режима");
        }
    });
    
    connect(m_settingsButton, &QPushButton::clicked, this, [this]() {
        WhisperModelSettingsDialog dlg(this);
        dlg.exec();
    });
    
    connect(m_subtitlesButton, &QPushButton::clicked, this, &SimpleMediaPlayer::createSubtitles);
    
    // Устанавливаем размер окна
    resize(800, 600);
    setWindowTitle("Simple Media Player");
    m_videoWidget->hide(); // Скрываем видео по умолчанию
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
    m_videoWidget->show(); // Показываем видео
    
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
    m_videoWidget->hide(); // Скрываем видео
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
    qDebug() << "SimpleMediaPlayer::dragEnterEvent called";
    
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        qDebug() << "SimpleMediaPlayer::dragEnterEvent - found" << urls.size() << "URLs";
        
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            QString filePath = urls.first().toLocalFile();
            qDebug() << "SimpleMediaPlayer::dragEnterEvent - accepting local file:" << filePath;
            event->acceptProposedAction();
        } else {
            qDebug() << "SimpleMediaPlayer::dragEnterEvent - not a local file, ignoring";
        }
    } else {
        qDebug() << "SimpleMediaPlayer::dragEnterEvent - no URLs found in mime data";
    }
}

void SimpleMediaPlayer::dragMoveEvent(QDragMoveEvent *event)
{
    qDebug() << "SimpleMediaPlayer::dragMoveEvent called";
    
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            event->acceptProposedAction();
        }
    }
}

void SimpleMediaPlayer::dropEvent(QDropEvent *event)
{
    qDebug() << "SimpleMediaPlayer::dropEvent called";
    
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        qDebug() << "SimpleMediaPlayer::dropEvent - found" << urls.size() << "URLs";
        
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            QString filePath = urls.first().toLocalFile();
            qDebug() << "SimpleMediaPlayer::dropEvent - opening file:" << filePath;
            
            if (openFile(filePath)) {
                qDebug() << "SimpleMediaPlayer::dropEvent - file opened successfully, starting playback";
                play();
            } else {
                qDebug() << "SimpleMediaPlayer::dropEvent - failed to open file";
            }
        } else {
            qDebug() << "SimpleMediaPlayer::dropEvent - not a local file, ignoring";
        }
    } else {
        qDebug() << "SimpleMediaPlayer::dropEvent - no URLs found in mime data";
    }
}

void SimpleMediaPlayer::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && isFullScreen()) {
        showNormal();
        if (m_fullscreenButton) {
            m_fullscreenButton->setText("⛶");
            m_fullscreenButton->setToolTip("Во весь экран");
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void SimpleMediaPlayer::createSubtitles()
{
    // Проверяем, есть ли открытый файл
    if (m_mediaPlayer->source().isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Сначала откройте видео файл");
        return;
    }
    
    QString videoPath = m_mediaPlayer->source().toLocalFile();
    if (videoPath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Не удалось получить путь к видео файлу");
        return;
    }
    
    // Получаем выбранную модель Whisper
    QSettings settings;
    QString selectedModel = settings.value("whisper/selected_model", "base").toString();
    
    // Проверяем, скачана ли модель
    QString projectDir = QDir::currentPath();
    QString modelPath = projectDir + "/models/whisper/ggml-" + selectedModel + ".bin";
    if (!QFile::exists(modelPath)) {
        QMessageBox::warning(this, "Ошибка", 
            QString("Модель '%1' не найдена по пути: %2\nСначала скачайте её в настройках Whisper.").arg(selectedModel).arg(modelPath));
        return;
    }
    
    // Создаем имя файла для субтитров
    QFileInfo videoFile(videoPath);
    QString defaultSubtitlesPath = videoFile.absolutePath() + "/" + videoFile.baseName();
    
    // Показываем диалог для выбора имени файла (без расширения)
    QString subtitlesPath = QFileDialog::getSaveFileName(
        this,
        "Сохранить субтитры как",
        defaultSubtitlesPath,
        "SRT файлы (*.srt);;Все файлы (*)"
    );
    
    if (subtitlesPath.isEmpty()) {
        // Пользователь отменил выбор
        return;
    }
    
    // Убираем расширение .srt если пользователь его добавил
    if (subtitlesPath.endsWith(".srt", Qt::CaseInsensitive)) {
        subtitlesPath = subtitlesPath.left(subtitlesPath.length() - 4);
    }
    
    // Приводим к абсолютному пути
    subtitlesPath = QFileInfo(subtitlesPath).absoluteFilePath();
    
    // Добавляем расширение .srt к выходному файлу
    QString subtitlesSrtPath = subtitlesPath + ".srt";
    
    // Проверяем, существует ли уже файл субтитров
    if (QFile::exists(subtitlesSrtPath)) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Файл существует", 
            QString("Файл субтитров '%1' уже существует. Перезаписать?").arg(QFileInfo(subtitlesSrtPath).fileName()),
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply == QMessageBox::No) {
            return;
        }
    }
    
    // Создаем диалог прогресса
    QProgressDialog progress("Создание субтитров...", "Отмена", 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);
    
    // Создаем временный аудио файл
    QString tempAudioPath = QDir::tempPath() + "/" + QFileInfo(videoPath).baseName() + "_temp.wav";
    
    // Извлекаем аудио из видео
    progress.setLabelText("Извлечение аудио из видео...");
    progress.setValue(10);
    
    QProcess *ffmpegProcess = new QProcess(this);
    QStringList ffmpegArgs;
    ffmpegArgs << "-i" << videoPath << "-vn" << "-acodec" << "pcm_s16le" << "-ar" << "16000" << "-ac" << "1" << tempAudioPath << "-y";
    
    ffmpegProcess->start("ffmpeg", ffmpegArgs);
    
    if (!ffmpegProcess->waitForStarted()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось запустить ffmpeg для извлечения аудио.");
        ffmpegProcess->deleteLater();
        return;
    }
    
    // Ждем завершения извлечения аудио
    if (!ffmpegProcess->waitForFinished(30000)) { // 30 секунд таймаут
        QMessageBox::critical(this, "Ошибка", "Таймаут при извлечении аудио.");
        ffmpegProcess->deleteLater();
        return;
    }
    
    if (ffmpegProcess->exitCode() != 0) {
        QString errorOutput = QString::fromUtf8(ffmpegProcess->readAllStandardError());
        QMessageBox::critical(this, "Ошибка", 
            QString("Ошибка при извлечении аудио:\n%1").arg(errorOutput));
        ffmpegProcess->deleteLater();
        return;
    }
    
    ffmpegProcess->deleteLater();
    
    progress.setValue(20);
    progress.setLabelText("Запуск Whisper для создания субтитров...");
    
    // Создаем процесс для Whisper
    QProcess *whisperProcess = new QProcess(this);
    
    // Подключаем сигналы для обновления прогресса
    connect(whisperProcess, &QProcess::started, [&progress]() {
        progress.setValue(10);
    });
    
    connect(whisperProcess, &QProcess::readyReadStandardOutput, [whisperProcess]() {
        QString output = QString::fromUtf8(whisperProcess->readAllStandardOutput());
        qDebug() << "Whisper stdout:" << output;
    });
    
    connect(whisperProcess, &QProcess::readyReadStandardError, [whisperProcess]() {
        QString error = QString::fromUtf8(whisperProcess->readAllStandardError());
        qDebug() << "Whisper stderr:" << error;
    });
    
    connect(whisperProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [this, &progress, whisperProcess, tempAudioPath, subtitlesSrtPath](int exitCode, QProcess::ExitStatus) {
            progress.setValue(100);
            qDebug() << "Whisper process finished with exit code:" << exitCode;
            
            if (QFile::exists(tempAudioPath)) {
                QFile::remove(tempAudioPath);
                qDebug() << "Temporary audio file removed:" << tempAudioPath;
            }
            
            progress.close();
            
            // Проверяем существование файла субтитров
            if (QFile::exists(subtitlesSrtPath)) {
                qDebug() << "Subtitles file created successfully:" << subtitlesSrtPath;
                QFileInfo fileInfo(subtitlesSrtPath);
                qDebug() << "File size:" << fileInfo.size() << "bytes";
            } else {
                qDebug() << "Subtitles file NOT found:" << subtitlesSrtPath;
                // Проверяем, может быть файл создался без расширения
                QString subtitlesPathWithoutExt = subtitlesSrtPath;
                subtitlesPathWithoutExt.chop(4); // убираем .srt
                if (QFile::exists(subtitlesPathWithoutExt)) {
                    qDebug() << "Found subtitles file without extension:" << subtitlesPathWithoutExt;
                    // Переименовываем файл
                    QFile::rename(subtitlesPathWithoutExt, subtitlesSrtPath);
                    qDebug() << "Renamed to:" << subtitlesSrtPath;
                } else {
                    qDebug() << "No subtitles file found with or without extension";
                }
            }
            
            if (exitCode != 0) {
                QString errorOutput = QString::fromUtf8(whisperProcess->readAllStandardError());
                QString stdOutput = QString::fromUtf8(whisperProcess->readAllStandardOutput());
                QMessageBox::critical(this, "Ошибка Whisper", 
                    QString("Файл субтитров не был создан.\n\nstderr:\n%1\n\nstdout:\n%2")
                    .arg(errorOutput).arg(stdOutput));
            } else if (!QFile::exists(subtitlesSrtPath)) {
                QString errorOutput = QString::fromUtf8(whisperProcess->readAllStandardError());
                QString stdOutput = QString::fromUtf8(whisperProcess->readAllStandardOutput());
                QMessageBox::critical(this, "Ошибка", 
                    QString("Файл субтитров не был создан.\n\nstderr:\n%1\n\nstdout:\n%2")
                    .arg(errorOutput).arg(stdOutput));
            }
            whisperProcess->deleteLater();
        });
    
    connect(whisperProcess, &QProcess::errorOccurred, [&progress, whisperProcess, this](QProcess::ProcessError error) {
        progress.setValue(100);
        progress.close();
        QMessageBox::critical(this, "Ошибка", 
            QString("Ошибка запуска Whisper: %1").arg(error));
        whisperProcess->deleteLater();
    });
    
    // Подключаем отмену
    connect(&progress, &QProgressDialog::canceled, [whisperProcess, ffmpegProcess, tempAudioPath, &progress]() {
        if (whisperProcess && whisperProcess->state() == QProcess::Running) {
            whisperProcess->terminate();
            whisperProcess->waitForFinished(5000);
            whisperProcess->kill();
        }
        if (ffmpegProcess && ffmpegProcess->state() == QProcess::Running) {
            ffmpegProcess->terminate();
            ffmpegProcess->waitForFinished(5000);
            ffmpegProcess->kill();
        }
        
        // Удаляем временный аудио файл при отмене
        if (QFile::exists(tempAudioPath)) {
            QFile::remove(tempAudioPath);
        }
        progress.close();
    });
    
    // Путь к локальному whisper
    QString whisperPath = QDir::currentPath() + "/../tools/whisper/whisper";
    
    // Формируем команду для Whisper
    QStringList args;
    args << "-m" << modelPath;
    args << "-f" << tempAudioPath;
    args << "-osrt";
    args << "-of" << subtitlesPath;  // Передаем путь БЕЗ расширения .srt
    args << "-l" << "ru";  // Устанавливаем русский язык
    args << "--max-len" << "300";  // Увеличиваем до 200 символов для интервалов ~10 секунд для субтитров
    args << "--split-on-word";
    args << "--word-thold" << "0.01";
    
    // Запускаем Whisper
    progress.setLabelText("Обработка аудио Whisper...");
    progress.setValue(50);
    
    qDebug() << "Starting Whisper with args:" << args;
    qDebug() << "Whisper path:" << whisperPath;
    qDebug() << "Model path:" << modelPath;
    qDebug() << "Audio path:" << tempAudioPath;
    qDebug() << "Output path:" << subtitlesSrtPath;
    
    whisperProcess->start(whisperPath, args);
    
    if (!whisperProcess->waitForStarted()) {
        progress.setValue(100);
        progress.close();
        QMessageBox::critical(this, "Ошибка", 
            QString("Не удалось запустить Whisper по пути: %1\nУбедитесь, что файл существует и имеет права на выполнение.").arg(whisperPath));
        return;
    }
    
    // Ждем завершения с таймаутом
    if (!whisperProcess->waitForFinished(300000)) { // 5 минут таймаут
        progress.setValue(100);
        progress.close();
        whisperProcess->terminate();
        whisperProcess->waitForFinished(10000);
        whisperProcess->kill();
        QMessageBox::critical(this, "Ошибка", "Whisper не завершился в течение 5 минут. Процесс прерван.");
        return;
    }
    
    // Показываем диалог прогресса (НЕ exec, а show)
    progress.show();
} 