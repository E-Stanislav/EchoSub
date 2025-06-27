package media

import (
	"fmt"
	"image"
	"image/png"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"sync"
	"time"
)

// InlineVideoPlayer представляет встроенный видеоплеер
type InlineVideoPlayer struct {
	mediaFile    *MediaFile
	frameRate    float64
	isPlaying    bool
	isPaused     bool
	position     time.Duration
	duration     time.Duration
	currentFrame int

	// Кэш кадров в памяти
	frameCache   map[int]*image.Image
	cacheMutex   sync.RWMutex
	maxCacheSize int

	// Предзагрузка кадров
	preloadChan chan int
	preloadStop chan bool

	// Канал для остановки воспроизведения
	stopChan chan bool

	// Мьютекс для синхронизации
	mu sync.Mutex

	// Callbacks
	onFrameReady       func(*image.Image)
	onError            func(error)
	onPlayStateChanged func(bool)
	onProgressChanged  func(float64) // Новый callback для прогресса
}

// NewInlineVideoPlayer создает новый встроенный видеоплеер
func NewInlineVideoPlayer() *InlineVideoPlayer {
	return &InlineVideoPlayer{
		stopChan:     make(chan bool),
		frameCache:   make(map[int]*image.Image),
		maxCacheSize: 50, // Увеличиваем кэш
		preloadChan:  make(chan int, 10),
		preloadStop:  make(chan bool),
	}
}

// LoadVideo загружает видео файл (без извлечения кадров)
func (p *InlineVideoPlayer) LoadVideo(mediaFile *MediaFile) error {
	p.mediaFile = mediaFile
	p.duration = mediaFile.Duration
	p.frameRate = 15.0 // Уменьшаем до 15 FPS для лучшей производительности
	p.currentFrame = 0
	p.position = 0

	// Очищаем кэш
	p.cacheMutex.Lock()
	p.frameCache = make(map[int]*image.Image)
	p.cacheMutex.Unlock()

	// Запускаем предзагрузку кадров
	go p.preloadFrames()

	return nil
}

// preloadFrames предзагружает кадры в фоне
func (p *InlineVideoPlayer) preloadFrames() {
	for {
		select {
		case frameNum := <-p.preloadChan:
			// Проверяем, есть ли уже в кэше
			p.cacheMutex.RLock()
			_, exists := p.frameCache[frameNum]
			p.cacheMutex.RUnlock()

			if !exists {
				// Извлекаем кадр
				if frame, err := p.extractFrame(frameNum); err == nil {
					p.cacheMutex.Lock()

					// Очищаем кэш если он переполнен
					if len(p.frameCache) >= p.maxCacheSize {
						// Удаляем старые кадры
						for k := range p.frameCache {
							delete(p.frameCache, k)
							break
						}
					}

					p.frameCache[frameNum] = frame
					p.cacheMutex.Unlock()
				}
			}
		case <-p.preloadStop:
			return
		}
	}
}

// extractFrame извлекает один кадр по требованию
func (p *InlineVideoPlayer) extractFrame(frameNumber int) (*image.Image, error) {
	if p.mediaFile == nil {
		return nil, fmt.Errorf("no video loaded")
	}

	frameTime := time.Duration(float64(frameNumber) / p.frameRate * float64(time.Second))
	outputPath := filepath.Join(os.TempDir(), fmt.Sprintf("echosub_frame_%d.png", frameNumber))

	var lastErr error
	for attempt := 0; attempt < 2; attempt++ {
		log.Printf("[InlineVideoPlayer] Extracting frame #%d at %.3fs (attempt %d)", frameNumber, frameTime.Seconds(), attempt+1)

		// Базовые параметры для быстрого старта
		args := []string{
			"-probesize", "32k", // Уменьшаем размер пробы
			"-analyzeduration", "0", // Пропускаем анализ длительности
			"-i", p.mediaFile.Path,
			"-ss", fmt.Sprintf("%.3f", frameTime.Seconds()),
			"-vframes", "1",
			"-q:v", "3",
			"-y",
			outputPath,
		}

		// Добавляем аппаратное ускорение если доступно (для macOS)
		// Можно расширить для других платформ
		hwAccelArgs := []string{"-hwaccel", "videotoolbox"}
		args = append(hwAccelArgs, args...)

		cmd := exec.Command("ffmpeg", args...)
		stderr, _ := cmd.CombinedOutput()
		if len(stderr) > 0 {
			log.Printf("[InlineVideoPlayer] ffmpeg stderr: %s", string(stderr))
		}
		if cmd.ProcessState != nil {
			log.Printf("[InlineVideoPlayer] ffmpeg exit code: %d", cmd.ProcessState.ExitCode())
		}
		if err := cmd.Run(); err != nil {
			lastErr = fmt.Errorf("ffmpeg failed: %w", err)
			log.Printf("[InlineVideoPlayer] ffmpeg error: %v", err)
			continue
		}

		// Проверяем размер файла
		fi, err := os.Stat(outputPath)
		if err != nil || fi.Size() < 100 {
			lastErr = fmt.Errorf("frame file too small or missing")
			log.Printf("[InlineVideoPlayer] frame file too small or missing: %v", err)
			time.Sleep(100 * time.Millisecond)
			continue
		}

		file, err := os.Open(outputPath)
		if err != nil {
			lastErr = fmt.Errorf("failed to open frame file: %w", err)
			log.Printf("[InlineVideoPlayer] failed to open frame file: %v", err)
			continue
		}
		defer file.Close()

		img, err := png.Decode(file)
		if err != nil {
			lastErr = fmt.Errorf("failed to decode frame: %w", err)
			log.Printf("[InlineVideoPlayer] failed to decode frame: %v", err)
			time.Sleep(100 * time.Millisecond)
			continue
		}

		os.Remove(outputPath)
		log.Printf("[InlineVideoPlayer] Frame #%d successfully extracted and decoded", frameNumber)
		return &img, nil
	}
	log.Printf("[InlineVideoPlayer] Failed to extract frame #%d: %v", frameNumber, lastErr)
	return nil, lastErr
}

// getFrame возвращает кадр (из кэша или извлекает новый)
func (p *InlineVideoPlayer) getFrame(frameNumber int) (*image.Image, error) {
	// Проверяем кэш
	p.cacheMutex.RLock()
	if cachedFrame, exists := p.frameCache[frameNumber]; exists {
		p.cacheMutex.RUnlock()
		return cachedFrame, nil
	}
	p.cacheMutex.RUnlock()

	// Извлекаем кадр
	frame, err := p.extractFrame(frameNumber)
	if err != nil {
		return nil, err
	}

	// Добавляем в кэш
	p.cacheMutex.Lock()

	// Очищаем кэш если он переполнен
	if len(p.frameCache) >= p.maxCacheSize {
		// Удаляем старые кадры
		for k := range p.frameCache {
			delete(p.frameCache, k)
			break // Удаляем только один
		}
	}

	p.frameCache[frameNumber] = frame
	p.cacheMutex.Unlock()

	// Запрашиваем предзагрузку следующих кадров
	go func() {
		for i := 1; i <= 3; i++ {
			select {
			case p.preloadChan <- frameNumber + i:
			default:
				return
			}
		}
	}()

	return frame, nil
}

// Play начинает воспроизведение
func (p *InlineVideoPlayer) Play() error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if p.mediaFile == nil {
		return fmt.Errorf("no video loaded")
	}

	if p.isPlaying && !p.isPaused {
		return nil // уже воспроизводится
	}

	p.isPlaying = true
	p.isPaused = false

	// Запускаем воспроизведение в горутине
	go p.playFrames()

	if p.onPlayStateChanged != nil {
		p.onPlayStateChanged(true)
	}

	return nil
}

// Pause приостанавливает воспроизведение
func (p *InlineVideoPlayer) Pause() {
	p.mu.Lock()
	defer p.mu.Unlock()

	p.isPaused = true

	if p.onPlayStateChanged != nil {
		p.onPlayStateChanged(false)
	}
}

// Stop останавливает воспроизведение
func (p *InlineVideoPlayer) Stop() {
	p.mu.Lock()
	defer p.mu.Unlock()

	p.isPlaying = false
	p.isPaused = false
	p.currentFrame = 0
	p.position = 0

	// Сигнализируем остановку
	select {
	case p.stopChan <- true:
	default:
	}

	if p.onPlayStateChanged != nil {
		p.onPlayStateChanged(false)
	}
}

// Seek перематывает к указанной позиции
func (p *InlineVideoPlayer) Seek(position time.Duration) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if p.mediaFile == nil {
		return fmt.Errorf("no video loaded")
	}

	if position < 0 || position > p.duration {
		return fmt.Errorf("position out of range")
	}

	p.position = position

	// Вычисляем номер кадра
	frameNumber := int(float64(position.Seconds()) * p.frameRate)
	if frameNumber < 0 {
		frameNumber = 0
	}

	p.currentFrame = frameNumber

	// Обновляем прогресс
	if p.onProgressChanged != nil {
		progress := float64(position) / float64(p.duration) * 100
		p.onProgressChanged(progress)
	}

	// Загружаем и отображаем кадр в горутине
	go func() {
		if frame, err := p.getFrame(frameNumber); err == nil {
			if p.onFrameReady != nil {
				p.onFrameReady(frame)
			}
		} else if p.onError != nil {
			p.onError(fmt.Errorf("failed to load frame: %w", err))
		}
	}()

	return nil
}

// playFrames воспроизводит кадры с заданной частотой
func (p *InlineVideoPlayer) playFrames() {
	frameInterval := time.Duration(1000/p.frameRate) * time.Millisecond
	maxFrames := int(p.duration.Seconds() * p.frameRate)

	for p.currentFrame < maxFrames {
		select {
		case <-p.stopChan:
			return
		default:
		}

		p.mu.Lock()
		if !p.isPlaying || p.isPaused {
			p.mu.Unlock()
			return
		}

		currentFrame := p.currentFrame
		p.currentFrame++
		p.position = time.Duration(float64(currentFrame)/p.frameRate) * time.Second

		// Обновляем прогресс
		if p.onProgressChanged != nil {
			progress := float64(p.position) / float64(p.duration) * 100
			p.onProgressChanged(progress)
		}

		p.mu.Unlock()

		// Загружаем и отображаем кадр в горутине
		go func(frameNum int) {
			if frame, err := p.getFrame(frameNum); err == nil {
				if p.onFrameReady != nil {
					p.onFrameReady(frame)
				}
			} else if p.onError != nil {
				p.onError(fmt.Errorf("failed to load frame %d: %w", frameNum, err))
			}
		}(currentFrame)

		// Ждем до следующего кадра
		time.Sleep(frameInterval)
	}

	// Воспроизведение завершено
	p.mu.Lock()
	p.isPlaying = false
	p.mu.Unlock()

	if p.onPlayStateChanged != nil {
		p.onPlayStateChanged(false)
	}
}

// GetPosition возвращает текущую позицию
func (p *InlineVideoPlayer) GetPosition() time.Duration {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.position
}

// GetDuration возвращает длительность
func (p *InlineVideoPlayer) GetDuration() time.Duration {
	return p.duration
}

// IsPlaying возвращает статус воспроизведения
func (p *InlineVideoPlayer) IsPlaying() bool {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.isPlaying && !p.isPaused
}

// IsPaused возвращает статус паузы
func (p *InlineVideoPlayer) IsPaused() bool {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.isPaused
}

// GetVideoInfo возвращает информацию о видео
func (p *InlineVideoPlayer) GetVideoInfo() (width, height int, fps float64, duration time.Duration) {
	if p.mediaFile != nil && p.mediaFile.HasVideo {
		return p.mediaFile.Width, p.mediaFile.Height, p.frameRate, p.duration
	}
	return 0, 0, 0, 0
}

// Close закрывает видеоплеер
func (p *InlineVideoPlayer) Close() error {
	p.Stop()

	// Останавливаем предзагрузку
	select {
	case p.preloadStop <- true:
	default:
	}

	// Очищаем кэш
	p.cacheMutex.Lock()
	p.frameCache = make(map[int]*image.Image)
	p.cacheMutex.Unlock()

	return nil
}

// SetCallbacks устанавливает callbacks
func (p *InlineVideoPlayer) SetCallbacks(
	onFrameReady func(*image.Image),
	onError func(error),
	onPlayStateChanged func(bool),
	onProgressChanged func(float64),
) {
	p.onFrameReady = onFrameReady
	p.onError = onError
	p.onPlayStateChanged = onPlayStateChanged
	p.onProgressChanged = onProgressChanged
}
