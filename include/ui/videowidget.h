#pragma once

#include <QWidget>
#include <QImage>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>

class VideoWidget : public QWidget {
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

    void setFrame(const QImage &frame);
    void clear();
    QSize getVideoSize() const { return m_videoSize; }

signals:
    void clicked();
    void doubleClicked();
    void seekRequested(qint64 position);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void updateScaledImage();
    qint64 positionFromMouse(const QPoint &pos) const;

    QImage m_currentFrame;
    QImage m_scaledFrame;
    QSize m_videoSize;
    bool m_hasFrame;
    bool m_keepAspectRatio;
}; 