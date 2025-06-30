#include "ui/videowidget.h"
#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsVideoItem>
#include <QGraphicsTextItem>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGraphicsRectItem>
#include <QGraphicsDropShadowEffect>

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

DraggableVideoWidget::DraggableVideoWidget(QWidget *parent)
    : QVideoWidget(parent) {
    setAcceptDrops(true);
    qDebug() << "DraggableVideoWidget created, parent:" << parent;
}

DraggableVideoWidget::~DraggableVideoWidget() {
    qDebug() << "DraggableVideoWidget destroyed";
}

void DraggableVideoWidget::setSubtitleText(const QString &text) {
    qDebug() << "DraggableVideoWidget::setSubtitleText called with:" << text;
    m_subtitleText = text;
    update();
}

void DraggableVideoWidget::setSubtitles(const QMap<qint64, QString> &subtitles) {
    qDebug() << "DraggableVideoWidget::setSubtitles called with" << subtitles.size() << "subtitles";
    m_subtitles = subtitles;
}

void DraggableVideoWidget::clearSubtitles() {
    qDebug() << "DraggableVideoWidget::clearSubtitles called";
    m_subtitles.clear();
    m_subtitleText.clear();
    update();
}

void DraggableVideoWidget::updateSubtitlePosition(qint64 position) {
    QString newText;
    QList<qint64> keys = m_subtitles.keys();
    std::sort(keys.begin(), keys.end());
    for (auto it = m_subtitles.begin(); it != m_subtitles.end(); ++it) {
        if (it.key() <= position && position < it.key() + 10000) {
            newText = it.value();
            break;
        }
    }
    if (newText != m_subtitleText) {
        qDebug() << "DraggableVideoWidget::updateSubtitlePosition new text:" << newText << "at position:" << position;
        m_subtitleText = newText;
        update();
    }
}

void DraggableVideoWidget::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            event->acceptProposedAction();
        }
    }
}

void DraggableVideoWidget::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            event->acceptProposedAction();
        }
    }
}

void DraggableVideoWidget::dropEvent(QDropEvent *event) {
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            QString filePath = urls.first().toLocalFile();
            emit fileDropped(filePath);
        }
    }
}

void DraggableVideoWidget::paintEvent(QPaintEvent *event) {
    qDebug() << "DraggableVideoWidget::paintEvent called, rect:" << rect() << "visible:" << isVisible();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // Сначала рисуем видео с прозрачностью
    painter.save();
    painter.setOpacity(0.5); // 50% прозрачности для видео
    QVideoWidget::paintEvent(event);
    painter.restore();
    // Затем рисуем субтитры
    if (!m_subtitleText.isEmpty()) {
        QFont subtitleFont = painter.font();
        subtitleFont.setPointSize(20);
        subtitleFont.setBold(true);
        painter.setFont(subtitleFont);
        QRect textRect = rect();
        textRect.setTop(textRect.bottom() - 150);
        painter.setPen(Qt::black);
        painter.drawText(textRect.translated(2,2), Qt::AlignCenter | Qt::TextWordWrap, m_subtitleText);
        painter.setPen(Qt::white);
        painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, m_subtitleText);
        qDebug() << "DraggableVideoWidget::paintEvent drew subtitle:" << m_subtitleText;
    }
}

// Реализация методов VideoGraphicsView
VideoGraphicsView::VideoGraphicsView(QWidget *parent)
    : QGraphicsView(parent), m_videoItem(new QGraphicsVideoItem()), m_subtitleItem(new QGraphicsTextItem()), m_subtitlesVisible(true) {
    setAcceptDrops(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *graphicsScene = new QGraphicsScene(this);
    setScene(graphicsScene);
    graphicsScene->addItem(m_videoItem);
    graphicsScene->addItem(m_subtitleItem);
    m_subtitleItem->setDefaultTextColor(Qt::yellow);
    m_videoItem->setZValue(0);
    m_subtitleItem->setZValue(2);
    m_subtitleItem->setPos(50, 50); // Центрирование можно доработать
    m_subtitleItem->setPlainText("");
    setFrameStyle(QFrame::NoFrame);
    fitInView(m_videoItem, Qt::KeepAspectRatio); // Инициализация
    // Подписка на изменение размера видео
    connect(m_videoItem, &QGraphicsVideoItem::nativeSizeChanged, this, [this]() {
        if (scene()) scene()->setSceneRect(m_videoItem->boundingRect());
        fitInView(m_videoItem, Qt::KeepAspectRatio);
    });
    // Добавляю поле для фона
    m_subtitleBg = nullptr;
}

void VideoGraphicsView::setSubtitles(const QMap<qint64, QString> &subtitles) {
    m_subtitles = subtitles;
}

void VideoGraphicsView::clearSubtitles() {
    m_subtitles.clear();
    m_subtitleItem->setPlainText("");
}

void VideoGraphicsView::setSubtitlesVisible(bool visible) {
    m_subtitlesVisible = visible;
    if (m_subtitleItem) {
        m_subtitleItem->setVisible(visible);
    }
    if (m_subtitleBg) {
        m_subtitleBg->setVisible(visible);
    }
}

void VideoGraphicsView::updateSubtitlePosition(qint64 position) {
    QString text;
    for (auto it = m_subtitles.constBegin(); it != m_subtitles.constEnd(); ++it) {
        if (position >= it.key()) text = it.value();
        else break;
    }
    
    // Если субтитры скрыты, очищаем текст
    if (!m_subtitlesVisible) {
        text.clear();
    }
    
    m_subtitleItem->setPlainText(text);

    // Определяем количество строк
    int lineCount = text.count("\n") + 1;
    int fontSize = 7;

    // Оформление субтитров
    QFont font = m_subtitleItem->font();
    font.setPointSize(fontSize);
    font.setBold(true);
    m_subtitleItem->setFont(font);
    m_subtitleItem->setDefaultTextColor(QColor(255,255,255));
    qreal maxWidth = scene() ? scene()->width() * 0.8 : 800;
    m_subtitleItem->setTextWidth(maxWidth); // Ограничиваем ширину

    // Центрирование и позиция внизу (отступ 15px)
    QRectF videoRect = m_videoItem->boundingRect();
    QRectF textRect = m_subtitleItem->boundingRect();
    qreal x = videoRect.x() + (videoRect.width() - textRect.width()) / 2;
    qreal y = videoRect.y() + videoRect.height() - textRect.height() - 15; // 15px отступ от низа
    m_subtitleItem->setPos(x, y);
    m_subtitleItem->setZValue(0);

    // Удаляем старый фон, если был
    if (m_subtitleBg) {
        if (scene()) scene()->removeItem(m_subtitleBg);
        delete m_subtitleBg;
        m_subtitleBg = nullptr;
    }
    // Добавляем новый фон под текст только если субтитры видимы и есть текст
    if (!text.isEmpty() && m_subtitlesVisible) {
        QRectF bgRect = m_subtitleItem->boundingRect().adjusted(-16, -8, 16, 8);
        // Делаем минимальную высоту фона
        if (bgRect.height() < 32) {
            bgRect.setHeight(32);
        }
        m_subtitleBg = new QGraphicsRectItem(bgRect);
        m_subtitleBg->setBrush(QColor(0,0,0,200));
        m_subtitleBg->setPen(Qt::NoPen);
        m_subtitleBg->setZValue(1);
        m_subtitleBg->setPos(x, y);
        if (scene()) scene()->addItem(m_subtitleBg);
    }
    // Явно выставляем zValue для текста и фона
    if (m_subtitleBg) m_subtitleBg->setZValue(1);
    m_subtitleItem->setZValue(2);
}

QGraphicsVideoItem* VideoGraphicsView::videoItem() const {
    return m_videoItem;
}

void VideoGraphicsView::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    fitInView(m_videoItem, Qt::KeepAspectRatio); // Подгоняем видео при изменении размера окна
}

#include "videowidget.moc"
