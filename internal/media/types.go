package media

import (
	"time"
)

// MediaFile представляет импортированный медиа-файл
type MediaFile struct {
	Path       string        // Путь к файлу
	Name       string        // Имя файла
	Format     string        // Формат (mp4, mkv, mov, mp3, wav, m4a, flac)
	Duration   time.Duration // Длительность
	Size       int64         // Размер в байтах
	HasVideo   bool          // Есть ли видео
	HasAudio   bool          // Есть ли аудио
	VideoCodec string        // Кодек видео
	AudioCodec string        // Кодек аудио
	SampleRate int           // Частота дискретизации аудио
	Channels   int           // Количество каналов
	Width      int           // Ширина видео (если есть)
	Height     int           // Высота видео (если есть)

	// UI-данные
	CoverPath    string // Путь к извлеченной обложке
	PosterFrame  string // Путь к первому кадру видео
	WaveformPath string // Путь к сгенерированной волнограмме
}

// MediaInfo содержит метаданные медиа-файла
type MediaInfo struct {
	File          *MediaFile
	IsLoading     bool  // Загружается ли файл
	LoadError     error // Ошибка загрузки
	WaveformReady bool  // Готова ли волнограмма
}

// SupportedFormats список поддерживаемых форматов для v1.0
var SupportedFormats = map[string]bool{
	"mp4":  true,
	"mkv":  true,
	"mov":  true,
	"mp3":  true,
	"wav":  true,
	"m4a":  true,
	"flac": true,
}

// IsSupportedFormat проверяет, поддерживается ли формат
func IsSupportedFormat(format string) bool {
	return SupportedFormats[format]
}
