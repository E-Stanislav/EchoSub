#pragma once

#include <QWidget>
#include <QImage>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMap>
#include <QVideoWidget>
#include <QPainter>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QGraphicsView>
#include <QGraphicsVideoItem>
#include <QGraphicsTextItem>
#include <QGraphicsRectItem>
#include <QString>

class VideoWidget : public QWidget {
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    virtual ~VideoWidget();

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

class DraggableVideoWidget : public QVideoWidget {
    Q_OBJECT
public:
    explicit DraggableVideoWidget(QWidget *parent = nullptr);
    virtual ~DraggableVideoWidget();
    void setSubtitleText(const QString &text);
    void setSubtitles(const QMap<qint64, QString> &subtitles);
    void clearSubtitles();
    void updateSubtitlePosition(qint64 position);

signals:
    void fileDropped(const QString &filePath);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_subtitleText;
    QMap<qint64, QString> m_subtitles;
};

class VideoGraphicsView : public QGraphicsView {
public:
    explicit VideoGraphicsView(QWidget *parent = nullptr);
    void setSubtitles(const QMap<qint64, QString> &subtitles);
    void updateSubtitlePosition(qint64 position);
    void clearSubtitles();
    void setSubtitlesVisible(bool visible);
    QGraphicsVideoItem* videoItem() const;
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    QGraphicsVideoItem *m_videoItem;
    QGraphicsTextItem *m_subtitleItem;
    QGraphicsRectItem *m_subtitleBg;
    QMap<qint64, QString> m_subtitles;
    bool m_subtitlesVisible;
}; 