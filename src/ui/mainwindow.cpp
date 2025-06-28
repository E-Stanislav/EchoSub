#include "ui/mainwindow.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_videoWidget(nullptr)
    , m_controlsPanel(nullptr)
    , m_statusBar(nullptr)
    , m_statusLabel(nullptr)
    , m_progressBar(nullptr)
    , m_menuBar(nullptr)
    , m_toolBar(nullptr)
    , m_openAction(nullptr)
    , m_playAction(nullptr)
    , m_pauseAction(nullptr)
    , m_stopAction(nullptr)
    , m_exitAction(nullptr)
    , m_mediaPlayer(nullptr)
    , m_isPlaying(false)
{
    // Initialize media player FIRST
    m_mediaPlayer = new MediaPlayer(this);
    
    setupUI();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupCentralWidget();
    
    connect(m_mediaPlayer, &MediaPlayer::playbackStateChanged, this, &MainWindow::onPlaybackStateChanged);
    connect(m_mediaPlayer, &MediaPlayer::frameReady, this, &MainWindow::onVideoFrameReady);
    connect(m_mediaPlayer, &MediaPlayer::positionChanged, this, &MainWindow::onPositionChanged);
    connect(m_mediaPlayer, &MediaPlayer::durationChanged, this, &MainWindow::onDurationChanged);
    connect(m_mediaPlayer, &MediaPlayer::error, this, &MainWindow::onError);
    connect(m_mediaPlayer, &MediaPlayer::fileLoaded, this, &MainWindow::onFileLoaded);
    
    // Connect controls
    if (m_controlsPanel) {
        connect(m_controlsPanel, &ControlsPanel::playClicked, m_mediaPlayer, &MediaPlayer::play);
        connect(m_controlsPanel, &ControlsPanel::pauseClicked, m_mediaPlayer, &MediaPlayer::pause);
        connect(m_controlsPanel, &ControlsPanel::stopClicked, m_mediaPlayer, &MediaPlayer::stop);
        connect(m_controlsPanel, &ControlsPanel::seekRequested, m_mediaPlayer, &MediaPlayer::seek);
    }
    
    // Enable drag and drop
    setAcceptDrops(true);
    
    // Set window properties
    setWindowTitle("EchoSub - Modern Media Player");
    resize(900, 700);
    setMinimumSize(600, 400);
}

MainWindow::~MainWindow()
{
    qDebug() << "MainWindow::~MainWindow() called";
}

void MainWindow::loadFile(const QString &filePath)
{
    if (m_mediaPlayer) {
        m_mediaPlayer->loadFile(filePath);
    }
}

void MainWindow::openFile()
{
    QString filePath = QFileDialog::getOpenFileName(this,
        "Open Media File",
        QDir::homePath(),
        "Media Files (*.mp3 *.wav *.mp4 *.avi *.mov *.mkv *.webm *.flv *.wmv *.flac *.aac *.ogg *.m4a);;"
        "Audio Files (*.mp3 *.wav *.flac *.aac *.ogg *.m4a);;"
        "Video Files (*.mp4 *.avi *.mov *.mkv *.webm *.flv *.wmv);;"
        "All Files (*.*)");
    
    if (!filePath.isEmpty()) {
        loadFile(filePath);
    }
}

void MainWindow::onFileLoaded(const QString &filePath)
{
    m_currentFile = filePath;
    updateWindowTitle();
    updateStatusBar();
    enableControls(true);
}

void MainWindow::onPlaybackStateChanged(bool isPlaying)
{
    m_isPlaying = isPlaying;
    updateStatusBar();
}

void MainWindow::onPositionChanged(qint64 position)
{
    if (m_controlsPanel) {
        m_controlsPanel->setPosition(position);
    }
    updateStatusBar();
}

void MainWindow::onDurationChanged(qint64 duration)
{
    if (m_controlsPanel) {
        m_controlsPanel->setDuration(duration);
    }
    updateStatusBar();
}

void MainWindow::onError(const QString &error)
{
    showError(error);
}

void MainWindow::onVideoFrameReady(const QImage &frame)
{
    if (m_videoWidget) {
        m_videoWidget->setFrame(frame);
    }
}

void MainWindow::onSeekRequested(qint64 position)
{
    if (m_mediaPlayer) {
        m_mediaPlayer->seek(position);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        if (!urls.isEmpty()) {
            QString filePath = urls.first().toLocalFile();
            loadFile(filePath);
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_mediaPlayer) {
        m_mediaPlayer->close();
    }
    event->accept();
}

void MainWindow::setupUI()
{
    // This will be called from constructor
}

void MainWindow::setupMenuBar()
{
    m_menuBar = menuBar();
    
    // File menu
    QMenu *fileMenu = m_menuBar->addMenu("&File");
    m_openAction = fileMenu->addAction("&Open...", this, &MainWindow::openFile, QKeySequence::Open);
    fileMenu->addSeparator();
    m_exitAction = fileMenu->addAction("E&xit", this, &QWidget::close, QKeySequence::Quit);
    
    // Playback menu
    QMenu *playbackMenu = m_menuBar->addMenu("&Playback");
    m_playAction = playbackMenu->addAction("&Play", m_mediaPlayer, &MediaPlayer::play, QKeySequence(Qt::Key_Space));
    m_pauseAction = playbackMenu->addAction("&Pause", m_mediaPlayer, &MediaPlayer::pause);
    m_stopAction = playbackMenu->addAction("&Stop", m_mediaPlayer, &MediaPlayer::stop, QKeySequence(Qt::Key_S));
}

void MainWindow::setupToolBar()
{
    m_toolBar = addToolBar("Main Toolbar");
    m_toolBar->addAction(m_openAction);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_playAction);
    m_toolBar->addAction(m_pauseAction);
    m_toolBar->addAction(m_stopAction);
}

void MainWindow::setupStatusBar()
{
    m_statusBar = statusBar();
    m_statusLabel = new QLabel("Ready");
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    
    m_statusBar->addWidget(m_statusLabel);
    m_statusBar->addPermanentWidget(m_progressBar);
}

void MainWindow::setupCentralWidget()
{
    m_centralWidget = new QWidget();
    setCentralWidget(m_centralWidget);
    
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    
    // Create video widget (placeholder for now)
    m_videoWidget = new VideoWidget();
    m_mainLayout->addWidget(m_videoWidget);
    
    // Create controls panel (placeholder for now)
    m_controlsPanel = new ControlsPanel();
    m_mainLayout->addWidget(m_controlsPanel);
    
    // Connect video widget signals
    if (m_videoWidget) {
        connect(m_videoWidget, &VideoWidget::seekRequested, this, &MainWindow::onSeekRequested);
    }
}

void MainWindow::updateWindowTitle()
{
    QString title = "EchoSub - Modern Media Player";
    if (!m_currentFile.isEmpty()) {
        title += " - " + QFileInfo(m_currentFile).fileName();
    }
    setWindowTitle(title);
}

void MainWindow::updateStatusBar()
{
    if (!m_statusLabel) return;
    if (!m_currentFile.isEmpty()) {
        QString status = QString("File: %1").arg(QFileInfo(m_currentFile).fileName());
        if (m_isPlaying) {
            status += " | Playing";
        }
        m_statusLabel->setText(status);
    } else {
        m_statusLabel->setText("Ready");
    }
}

void MainWindow::enableControls(bool enable)
{
    if (m_controlsPanel) {
        m_controlsPanel->enableControls(enable);
    }
    
    if (m_playAction) m_playAction->setEnabled(enable);
    if (m_pauseAction) m_pauseAction->setEnabled(enable);
    if (m_stopAction) m_stopAction->setEnabled(enable);
}

void MainWindow::showError(const QString &error)
{
    QMessageBox::critical(this, "Error", error);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
        // Toggle play/pause
        if (m_mediaPlayer) {
            if (m_isPlaying) {
                m_mediaPlayer->pause();
            } else {
                m_mediaPlayer->play();
            }
        }
        event->accept();
        break;
    case Qt::Key_Left:
        // Seek back 10 seconds
        if (m_mediaPlayer) {
            qint64 currentPos = m_mediaPlayer->getPosition();
            qint64 newPos = qMax(0LL, currentPos - 10000);
            m_mediaPlayer->seek(newPos);
        }
        event->accept();
        break;
    case Qt::Key_Right:
        // Seek forward 10 seconds
        if (m_mediaPlayer) {
            qint64 currentPos = m_mediaPlayer->getPosition();
            qint64 duration = m_mediaPlayer->getDuration();
            qint64 newPos = qMin(duration, currentPos + 10000);
            m_mediaPlayer->seek(newPos);
        }
        event->accept();
        break;
    case Qt::Key_F:
        // Command + F for fullscreen toggle
        if (event->modifiers() & Qt::ControlModifier) {
            if (isFullScreen()) {
                showNormal();
            } else {
                showFullScreen();
            }
            event->accept();
        } else {
            QMainWindow::keyPressEvent(event);
        }
        break;
    case Qt::Key_Up:
        // Increase volume (if controls panel supports it)
        if (m_controlsPanel) {
            // This would need to be implemented in ControlsPanel
            // For now, just accept the event
        }
        event->accept();
        break;
    case Qt::Key_Down:
        // Decrease volume (if controls panel supports it)
        if (m_controlsPanel) {
            // This would need to be implemented in ControlsPanel
            // For now, just accept the event
        }
        event->accept();
        break;
    default:
        QMainWindow::keyPressEvent(event);
        break;
    }
}
