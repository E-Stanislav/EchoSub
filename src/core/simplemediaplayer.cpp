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
#include <QPainter>
#include <QTextDocument>
#include <QRegularExpression>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <cmath>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QLabel>

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
    
    // Создаем overlay для субтитров как дочерний виджет SimpleMediaPlayer
    m_subtitleOverlay = new QWidget(this);
    m_subtitleOverlay->setStyleSheet("background: rgba(0,0,255,0.3); border: 2px solid red;");
    m_subtitleOverlay->setAttribute(Qt::WA_TranslucentBackground);
    m_subtitleOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_subtitleOverlay->setGeometry(50, 50, 600, 100); // фиксированная позиция и размер для отладки
    m_subtitleOverlay->show();
    m_subtitleOverlay->raise();
    
    // Добавляем QLabel для субтитров
    m_subtitleLabel = new QLabel("ТЕСТ СУБТИТРОВ", m_subtitleOverlay);
    m_subtitleLabel->setStyleSheet("color: yellow; font-size: 32px; font-weight: bold; background: rgba(0,0,0,0.7); border: 2px solid green;");
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->show();
    QVBoxLayout *overlayLayout = new QVBoxLayout(m_subtitleOverlay);
    overlayLayout->addWidget(m_subtitleLabel);
    overlayLayout->setContentsMargins(10, 10, 10, 10);
    
    qDebug() << "SimpleMediaPlayer: SubtitleOverlay created as child of main window (DEBUG COLORS)";
    
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
    
    m_subtitlesOverlayButton = new QPushButton(this);
    m_subtitlesOverlayButton->setText("📝");
    m_subtitlesOverlayButton->setToolTip("Создать субтитры поверх видео (Whisper)");
    
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
    controlsLayout->addWidget(m_subtitlesOverlayButton);
    
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
    
    connect(m_subtitlesOverlayButton, &QPushButton::clicked, this, &SimpleMediaPlayer::createSubtitlesOverlay);
    
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
    
    // Позиционируем overlay поверх видео
    if (m_subtitleOverlay && m_videoWidget) {
        QRect videoRect = m_videoWidget->geometry();
        m_subtitleOverlay->setGeometry(videoRect);
        m_subtitleOverlay->raise();
        qDebug() << "SimpleMediaPlayer: openFile, overlay repositioned to" << videoRect;
    }
    
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
    if (m_videoWidget) {
        m_videoWidget->updateSubtitlePosition(position);
    }
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

void SimpleMediaPlayer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_subtitleOverlay && m_videoWidget) {
        QRect videoRect = m_videoWidget->geometry();
        m_subtitleOverlay->setGeometry(videoRect);
        m_subtitleOverlay->raise();
        qDebug() << "SimpleMediaPlayer: window resized, overlay repositioned to" << videoRect;
    }
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
    
    // Переменная для хранения данных субтитров
    QByteArray srtData;
    
    // Подключаем сигналы для обновления прогресса
    connect(whisperProcess, &QProcess::started, [&progress]() {
        progress.setValue(30);
    });
    
    connect(whisperProcess, &QProcess::readyReadStandardOutput, [whisperProcess, &srtData]() {
        QByteArray output = whisperProcess->readAllStandardOutput();
        srtData.append(output);
        qDebug() << "Whisper stdout:" << QString::fromUtf8(output);
    });
    
    connect(whisperProcess, &QProcess::readyReadStandardError, [whisperProcess]() {
        QString error = QString::fromUtf8(whisperProcess->readAllStandardError());
        qDebug() << "Whisper stderr:" << error;
    });
    
    connect(whisperProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [this, &progress, whisperProcess, tempAudioPath, &srtData](int exitCode, QProcess::ExitStatus) {
            progress.setValue(100);
            qDebug() << "Whisper process finished with exit code:" << exitCode;
            
            // Удаляем временные файлы
            if (QFile::exists(tempAudioPath)) {
                QFile::remove(tempAudioPath);
                qDebug() << "Temporary audio file removed:" << tempAudioPath;
            }
            
            progress.close();
            
            if (exitCode == 0 && !srtData.isEmpty()) {
                // Парсим субтитры и отображаем их
                QMap<qint64, QString> subtitles = parseSrtData(srtData);
                if (!subtitles.isEmpty()) {
                    displaySubtitles(subtitles);
                    QMessageBox::information(this, "Успех", 
                        QString("Субтитры созданы и отображаются поверх видео!\nСоздано %1 сегментов.").arg(subtitles.size()));
                } else {
                    QMessageBox::warning(this, "Предупреждение", "Субтитры созданы, но не удалось их распарсить.");
                }
            } else {
                QString errorOutput = QString::fromUtf8(whisperProcess->readAllStandardError());
                QString stdOutput = QString::fromUtf8(whisperProcess->readAllStandardOutput());
                QMessageBox::critical(this, "Ошибка Whisper", 
                    QString("Не удалось создать субтитры.\n\nstderr:\n%1\n\nstdout:\n%2")
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
    
    // Формируем команду для Whisper с параметрами для субтитров поверх видео
    QStringList args;
    args << "-m" << modelPath;
    args << "-f" << tempAudioPath;
    args << "-osrt";
    args << "-of" << subtitlesSrtPath;  // Передаем путь БЕЗ расширения .srt
    args << "-l" << "ru";  // Устанавливаем русский язык
    args << "--max-len" << "300";  // Длинные интервалы для субтитров поверх видео
    args << "--split-on-word";
    args << "--word-thold" << "0.01";
    
    // Запускаем Whisper
    progress.setLabelText("Обработка аудио Whisper для субтитров поверх видео...");
    progress.setValue(50);
    
    qDebug() << "Starting Whisper for overlay subtitles with args:" << args;
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

void SimpleMediaPlayer::createSubtitlesOverlay()
{
    qDebug() << "createSubtitlesOverlay: starting...";
    if (m_mediaPlayer->source().isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Сначала откройте видео файл");
        return;
    }
    QString videoPath = m_mediaPlayer->source().toLocalFile();
    if (videoPath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Не удалось получить путь к видео файлу");
        return;
    }
    qDebug() << "createSubtitlesOverlay: video path:" << videoPath;
    QSettings settings;
    QString selectedModel = settings.value("whisper/selected_model", "base").toString();
    QString projectDir = QDir::currentPath();
    QString modelPath = projectDir + "/models/whisper/ggml-" + selectedModel + ".bin";
    if (!QFile::exists(modelPath)) {
        QMessageBox::warning(this, "Ошибка", QString("Модель '%1' не найдена по пути: %2\nСначала скачайте её в настройках Whisper.").arg(selectedModel).arg(modelPath));
        return;
    }
    qDebug() << "createSubtitlesOverlay: model path:" << modelPath;
    QString tempAudioPath = QDir::tempPath() + "/" + QFileInfo(videoPath).baseName() + "_temp.wav";
    qDebug() << "createSubtitlesOverlay: extracting audio to:" << tempAudioPath;
    // Извлекаем аудио
    QProcess ffmpegProcess;
    QStringList ffmpegArgs;
    ffmpegArgs << "-i" << videoPath << "-vn" << "-acodec" << "pcm_s16le" << "-ar" << "16000" << "-ac" << "1" << tempAudioPath << "-y";
    ffmpegProcess.start("ffmpeg", ffmpegArgs);
    if (!ffmpegProcess.waitForStarted() || !ffmpegProcess.waitForFinished(30000) || ffmpegProcess.exitCode() != 0) {
        QMessageBox::critical(this, "Ошибка", "Не удалось извлечь аудио");
        return;
    }
    qDebug() << "createSubtitlesOverlay: audio extraction completed";
    // Получаем длительность
    QProcess durationProcess;
    durationProcess.start("ffprobe", QStringList{"-i", tempAudioPath, "-show_entries", "format=duration", "-v", "quiet", "-of", "csv=p=0"});
    durationProcess.waitForFinished(5000);
    double totalDuration = durationProcess.readAllStandardOutput().trimmed().toDouble();
    if (totalDuration <= 0) {
        QMessageBox::critical(this, "Ошибка", "Не удалось получить длительность аудио");
        return;
    }
    qDebug() << "createSubtitlesOverlay: total duration:" << totalDuration << "seconds";
    // Чанки
    const int chunkDuration = 15;
    const int overlapDuration = 2;
    const int totalChunks = static_cast<int>(std::ceil(totalDuration / (chunkDuration - overlapDuration)));
    qDebug() << "createSubtitlesOverlay: will process" << totalChunks << "chunks";
    if (m_videoWidget) m_videoWidget->clearSubtitles();
    QMap<qint64, QString> *allSubtitles = new QMap<qint64, QString>();
    QString whisperPath = QDir::currentPath() + "/../tools/whisper/whisper";
    qDebug() << "createSubtitlesOverlay: whisper path:" << whisperPath;
    // Последовательная обработка чанков
    auto processNextChunk = [=](int chunkIndex, auto &&processNextChunkRef) mutable -> void {
        qDebug() << "createSubtitlesOverlay: processing chunk" << chunkIndex << "of" << totalChunks;
        if (chunkIndex >= totalChunks) {
            // Всё готово
            qDebug() << "createSubtitlesOverlay: all chunks processed, finalizing...";
            if (QFile::exists(tempAudioPath)) QFile::remove(tempAudioPath);
            if (!allSubtitles->isEmpty()) {
                qDebug() << "createSubtitlesOverlay: setting final subtitles, count:" << allSubtitles->size();
                m_videoWidget->setSubtitles(*allSubtitles);
                QMessageBox::information(this, "Успех", QString("Субтитры созданы и отображаются поверх видео!\nОбработано %1 чанков, создано %2 сегментов.").arg(totalChunks).arg(allSubtitles->size()));
            } else {
                QMessageBox::warning(this, "Предупреждение", "Не удалось создать субтитры.");
            }
            delete allSubtitles;
            return;
        }
        double startTime = chunkIndex * (chunkDuration - overlapDuration);
        double endTime = std::min(startTime + chunkDuration, totalDuration);
        QString chunkTempPath = QDir::tempPath() + "/" + QFileInfo(videoPath).baseName() + QString("_chunk_%1").arg(chunkIndex);
        qDebug() << "createSubtitlesOverlay: chunk" << chunkIndex << "time range:" << startTime << "-" << endTime;
        QProcess *chunkFfmpeg = new QProcess(this);
        QStringList chunkArgs;
        chunkArgs << "-i" << tempAudioPath << "-ss" << QString::number(startTime) << "-t" << QString::number(endTime - startTime)
                  << "-acodec" << "pcm_s16le" << "-ar" << "16000" << "-ac" << "1" << chunkTempPath + ".wav" << "-y";
        connect(chunkFfmpeg, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int, QProcess::ExitStatus) mutable {
            chunkFfmpeg->deleteLater();
            if (!QFile::exists(chunkTempPath + ".wav")) {
                qDebug() << "createSubtitlesOverlay: chunk" << chunkIndex << "ffmpeg failed, skipping to next";
                processNextChunkRef(chunkIndex + 1, processNextChunkRef);
                return;
            }
            qDebug() << "createSubtitlesOverlay: chunk" << chunkIndex << "ffmpeg completed, starting whisper";
            QProcess *chunkWhisper = new QProcess(this);
            QStringList whisperArgs;
            whisperArgs << "-m" << modelPath << "-f" << chunkTempPath + ".wav" << "-osrt" << "-of" << chunkTempPath << "-l" << "ru" << "--max-len" << "300" << "--split-on-word" << "--word-thold" << "0.01";
            connect(chunkWhisper, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [=](int, QProcess::ExitStatus) mutable {
                QString srtFilePath = chunkTempPath + ".srt";
                if (QFile::exists(srtFilePath)) {
                    qDebug() << "createSubtitlesOverlay: chunk" << chunkIndex << "whisper completed, parsing SRT";
                    QFile srtFile(srtFilePath);
                    if (srtFile.open(QIODevice::ReadOnly)) {
                        QByteArray srtData = srtFile.readAll();
                        srtFile.close();
                        QMap<qint64, QString> chunkSubtitles = parseSrtData(srtData);
                        qint64 timeOffset = static_cast<qint64>(startTime * 1000);
                        for (auto it = chunkSubtitles.begin(); it != chunkSubtitles.end(); ++it) {
                            qint64 adjustedTime = it.key() + timeOffset;
                            (*allSubtitles)[adjustedTime] = it.value();
                        }
                        qDebug() << "createSubtitlesOverlay: chunk" << chunkIndex << "added" << chunkSubtitles.size() << "subtitles, total now:" << allSubtitles->size();
                        // Обновляем overlay после каждого чанка
                        if (m_videoWidget) {
                            m_videoWidget->setSubtitles(*allSubtitles);
                            qDebug() << "createSubtitlesOverlay: chunk" << chunkIndex << "overlay updated";
                        }
                    }
                } else {
                    qDebug() << "createSubtitlesOverlay: chunk" << chunkIndex << "whisper failed, no SRT file created";
                }
                QFile::remove(chunkTempPath + ".wav");
                QFile::remove(srtFilePath);
                chunkWhisper->deleteLater();
                // Следующий чанк
                QTimer::singleShot(0, this, [=]() mutable { processNextChunkRef(chunkIndex + 1, processNextChunkRef); });
            });
            chunkWhisper->start(whisperPath, whisperArgs);
        });
        chunkFfmpeg->start("ffmpeg", chunkArgs);
    };
    // Запуск первого чанка
    qDebug() << "createSubtitlesOverlay: starting first chunk";
    processNextChunk(0, processNextChunk);
}

// Методы для работы с субтитрами
QMap<qint64, QString> SimpleMediaPlayer::parseSrtData(const QByteArray &srtData)
{
    QMap<qint64, QString> subtitles;
    QString srtText = QString::fromUtf8(srtData);
    // Регулярное выражение для парсинга SRT
    QRegularExpression regex(
        "(\\d+)\\s+" // номер
        "(\\d{2}):(\\d{2}):(\\d{2}),(\\d{3})\\s+-->\\s+" // время начала
        "(\\d{2}):(\\d{2}):(\\d{2}),(\\d{3})\\s+" // время конца
        "([\\s\\S]*?)(?=\\n\\d+\\n|$)" // текст (группа 10)
    );
    QRegularExpressionMatchIterator it = regex.globalMatch(srtText);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        // Извлекаем время начала
        int hours = match.captured(2).toInt();
        int minutes = match.captured(3).toInt();
        int seconds = match.captured(4).toInt();
        int milliseconds = match.captured(5).toInt();
        qint64 startTime = (hours * 3600 + minutes * 60 + seconds) * 1000 + milliseconds;
        // Извлекаем текст
        QString text = match.captured(10).trimmed();
        text = text.replace(QRegularExpression("\\n"), " ");
        subtitles[startTime] = text;
    }
    return subtitles;
}

void SimpleMediaPlayer::displaySubtitles(const QMap<qint64, QString> &subtitles)
{
    if (m_videoWidget) {
        m_videoWidget->setSubtitles(subtitles);
    }
} 