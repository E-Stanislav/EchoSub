package media

import (
	"fmt"
	"time"
)

// MediaPlayer представляет медиаплеер
type MediaPlayer struct {
	ffmpeg    *FFmpegProcessor
	mediaFile *MediaFile
	isPlaying bool
	isPaused  bool
	position  time.Duration
	duration  time.Duration

	// Callbacks
	onPlayStateChanged func(bool)
	onPositionChanged  func(time.Duration)
	onError            func(error)
}

// NewMediaPlayer создает новый медиаплеер
func NewMediaPlayer(ffmpeg *FFmpegProcessor) *MediaPlayer {
	return &MediaPlayer{
		ffmpeg: ffmpeg,
	}
}

// LoadMedia загружает медиа-файл в плеер
func (mp *MediaPlayer) LoadMedia(mediaFile *MediaFile) error {
	mp.mediaFile = mediaFile
	mp.duration = mediaFile.Duration
	mp.position = 0
	mp.isPlaying = false
	mp.isPaused = false

	return nil
}

// Play начинает воспроизведение
func (mp *MediaPlayer) Play() error {
	if mp.mediaFile == nil {
		return fmt.Errorf("no media loaded")
	}

	if mp.isPlaying && !mp.isPaused {
		return nil // уже воспроизводится
	}

	mp.isPlaying = true
	mp.isPaused = false

	// Запускаем воспроизведение в горутине
	go mp.playMedia()

	if mp.onPlayStateChanged != nil {
		mp.onPlayStateChanged(true)
	}

	return nil
}

// Pause приостанавливает воспроизведение
func (mp *MediaPlayer) Pause() {
	mp.isPaused = true
	if mp.onPlayStateChanged != nil {
		mp.onPlayStateChanged(false)
	}
}

// Stop останавливает воспроизведение
func (mp *MediaPlayer) Stop() {
	mp.isPlaying = false
	mp.isPaused = false
	mp.position = 0

	if mp.onPlayStateChanged != nil {
		mp.onPlayStateChanged(false)
	}
}

// Seek перематывает к указанной позиции
func (mp *MediaPlayer) Seek(position time.Duration) error {
	if mp.mediaFile == nil {
		return fmt.Errorf("no media loaded")
	}

	if position < 0 || position > mp.duration {
		return fmt.Errorf("position out of range")
	}

	mp.position = position

	if mp.onPositionChanged != nil {
		mp.onPositionChanged(position)
	}

	return nil
}

// GetPosition возвращает текущую позицию
func (mp *MediaPlayer) GetPosition() time.Duration {
	return mp.position
}

// GetDuration возвращает длительность
func (mp *MediaPlayer) GetDuration() time.Duration {
	return mp.duration
}

// IsPlaying возвращает статус воспроизведения
func (mp *MediaPlayer) IsPlaying() bool {
	return mp.isPlaying && !mp.isPaused
}

// IsPaused возвращает статус паузы
func (mp *MediaPlayer) IsPaused() bool {
	return mp.isPaused
}

// SetCallbacks устанавливает callbacks
func (mp *MediaPlayer) SetCallbacks(
	onPlayStateChanged func(bool),
	onPositionChanged func(time.Duration),
	onError func(error),
) {
	mp.onPlayStateChanged = onPlayStateChanged
	mp.onPositionChanged = onPositionChanged
	mp.onError = onError
}

// playMedia воспроизводит медиа через FFmpeg
func (mp *MediaPlayer) playMedia() {
	if mp.mediaFile == nil {
		return
	}

	// Если это видео, не воспроизводим
	if mp.mediaFile.HasVideo {
		if mp.onError != nil {
			mp.onError(fmt.Errorf("Video playback is handled by InlineVideoPlayer. MediaPlayer is for audio only."))
		}
		mp.isPlaying = false
		mp.isPaused = false
		if mp.onPlayStateChanged != nil {
			mp.onPlayStateChanged(false)
		}
		return
	}

	// TODO: Реализовать аудиоплеер без ffplay (например, через PortAudio)
	if mp.onError != nil {
		mp.onError(fmt.Errorf("Audio playback is not implemented (ffplay removed)."))
	}
	mp.isPlaying = false
	mp.isPaused = false
	if mp.onPlayStateChanged != nil {
		mp.onPlayStateChanged(false)
	}
}

// formatDuration форматирует длительность для FFmpeg
func formatDuration(d time.Duration) string {
	seconds := int(d.Seconds())
	hours := seconds / 3600
	minutes := (seconds % 3600) / 60
	secs := seconds % 60

	if hours > 0 {
		return fmt.Sprintf("%02d:%02d:%02d", hours, minutes, secs)
	}
	return fmt.Sprintf("%02d:%02d", minutes, secs)
}

// GetPlaybackInfo возвращает информацию о воспроизведении
func (mp *MediaPlayer) GetPlaybackInfo() PlaybackInfo {
	return PlaybackInfo{
		IsPlaying:  mp.IsPlaying(),
		IsPaused:   mp.IsPaused(),
		Position:   mp.position,
		Duration:   mp.duration,
		Progress:   mp.getProgress(),
		TimeString: mp.getTimeString(),
	}
}

// getProgress возвращает прогресс в процентах (0-100)
func (mp *MediaPlayer) getProgress() float64 {
	if mp.duration == 0 {
		return 0
	}
	return float64(mp.position) / float64(mp.duration) * 100
}

// getTimeString возвращает строку времени
func (mp *MediaPlayer) getTimeString() string {
	pos := formatDuration(mp.position)
	dur := formatDuration(mp.duration)
	return fmt.Sprintf("%s / %s", pos, dur)
}

// PlaybackInfo содержит информацию о воспроизведении
type PlaybackInfo struct {
	IsPlaying  bool
	IsPaused   bool
	Position   time.Duration
	Duration   time.Duration
	Progress   float64
	TimeString string
}
