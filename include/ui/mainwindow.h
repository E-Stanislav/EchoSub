#pragma once

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QProgressBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QKeyEvent>

#include "core/mediaplayer.h"
#include "ui/videowidget.h"
#include "ui/controlspanel.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void loadFile(const QString &filePath);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void openFile();
    void onFileLoaded(const QString &filePath);
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onError(const QString &error);
    void onVideoFrameReady(const QImage &frame);
    void onSeekRequested(qint64 position);
    void onPlaybackStateChanged(bool isPlaying);

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupCentralWidget();
    void updateWindowTitle();
    void updateStatusBar();
    void enableControls(bool enable);
    void showError(const QString &error);

    // UI Components
    QWidget *m_centralWidget;
    QVBoxLayout *m_mainLayout;
    
    // Video display
    VideoWidget *m_videoWidget;
    
    // Controls
    ControlsPanel *m_controlsPanel;
    
    // Status
    QStatusBar *m_statusBar;
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;
    
    // Menu and toolbar
    QMenuBar *m_menuBar;
    QToolBar *m_toolBar;
    
    // Actions
    QAction *m_openAction;
    QAction *m_playAction;
    QAction *m_pauseAction;
    QAction *m_stopAction;
    QAction *m_exitAction;
    
    // Media player
    MediaPlayer *m_mediaPlayer;
    
    // State
    QString m_currentFile;
    bool m_isPlaying;
}; 