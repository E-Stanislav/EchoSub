#include "ui/videowidget.h"
#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
    , m_hasFrame(false)
    , m_keepAspectRatio(true)
{
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    
    // Set background color
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
}

VideoWidget::~VideoWidget()
{
}

void VideoWidget::setFrame(const QImage &frame)
{
    m_currentFrame = frame;
    m_videoSize = frame.size();
    m_hasFrame = true;
    updateScaledImage();
    update();
}

void VideoWidget::clear()
{
    m_currentFrame = QImage();
    m_scaledFrame = QImage();
    m_hasFrame = false;
    update();
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    if (m_hasFrame && !m_scaledFrame.isNull()) {
        // Calculate position to center the image
        QRect widgetRect = rect();
        QRect imageRect = m_scaledFrame.rect();
        
        int x = (widgetRect.width() - imageRect.width()) / 2;
        int y = (widgetRect.height() - imageRect.height()) / 2;
        
        painter.drawImage(x, y, m_scaledFrame);
    } else {
        // Draw placeholder
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 16));
        painter.drawText(rect(), Qt::AlignCenter, "No Video");
    }
}

void VideoWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_hasFrame) {
        updateScaledImage();
    }
}

void VideoWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
        
        // Calculate position for seeking
        qint64 position = positionFromMouse(event->pos());
        emit seekRequested(position);
    }
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked();
    }
}

void VideoWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
        emit clicked(); // Toggle play/pause
        break;
    case Qt::Key_Left:
        emit seekRequested(-10000); // Seek back 10 seconds
        break;
    case Qt::Key_Right:
        emit seekRequested(10000); // Seek forward 10 seconds
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

void VideoWidget::updateScaledImage()
{
    if (!m_hasFrame || m_currentFrame.isNull()) {
        return;
    }
    
    QSize widgetSize = size();
    QSize imageSize = m_currentFrame.size();
    
    if (m_keepAspectRatio) {
        m_scaledFrame = m_currentFrame.scaled(widgetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        m_scaledFrame = m_currentFrame.scaled(widgetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
}

qint64 VideoWidget::positionFromMouse(const QPoint &pos) const
{
    // This is a placeholder - in a real implementation, you would calculate
    // the position based on the video duration and mouse position
    return 0;
}
