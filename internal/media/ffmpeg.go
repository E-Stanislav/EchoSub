package media

import (
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"
)

// FFmpegProcessor обрабатывает медиа-файлы через FFmpeg
type FFmpegProcessor struct {
	ffmpegPath string
	tempDir    string
}

// FFprobeOutput структура для парсинга JSON от ffprobe
type FFprobeOutput struct {
	Format  FormatInfo   `json:"format"`
	Streams []StreamInfo `json:"streams"`
}

// FormatInfo информация о формате файла
type FormatInfo struct {
	Duration string `json:"duration"`
	Size     string `json:"size"`
	BitRate  string `json:"bit_rate"`
}

// StreamInfo информация о потоке (видео/аудио)
type StreamInfo struct {
	CodecType  string `json:"codec_type"`
	CodecName  string `json:"codec_name"`
	Width      int    `json:"width,omitempty"`
	Height     int    `json:"height,omitempty"`
	SampleRate string `json:"sample_rate,omitempty"`
	Channels   int    `json:"channels,omitempty"`
}

// NewFFmpegProcessor создает новый процессор FFmpeg
func NewFFmpegProcessor() (*FFmpegProcessor, error) {
	// Проверяем установку FFmpeg
	if err := ValidateFFmpegInstallation(); err != nil {
		return nil, fmt.Errorf("FFmpeg validation failed: %w", err)
	}

	// Получаем путь к ffmpeg
	ffmpegPath, err := exec.LookPath("ffmpeg")
	if err != nil {
		return nil, fmt.Errorf("ffmpeg not found: %w", err)
	}

	// Создаем временную директорию
	tempDir, err := os.MkdirTemp("", "echosub-*")
	if err != nil {
		return nil, fmt.Errorf("failed to create temp dir: %w", err)
	}

	return &FFmpegProcessor{
		ffmpegPath: ffmpegPath,
		tempDir:    tempDir,
	}, nil
}

// LoadMediaFile загружает медиа-файл и извлекает метаданные
func (f *FFmpegProcessor) LoadMediaFile(filePath string) (*MediaFile, error) {
	// Проверяем существование файла
	if _, err := os.Stat(filePath); os.IsNotExist(err) {
		return nil, fmt.Errorf("file not found: %s", filePath)
	}

	// Получаем базовую информацию о файле
	fileInfo, err := os.Stat(filePath)
	if err != nil {
		return nil, fmt.Errorf("failed to get file info: %w", err)
	}

	// Извлекаем метаданные через FFmpeg
	metadata, err := f.extractMetadata(filePath)
	if err != nil {
		return nil, fmt.Errorf("failed to extract metadata: %w", err)
	}

	// Создаем объект MediaFile
	mediaFile := &MediaFile{
		Path:       filePath,
		Name:       filepath.Base(filePath),
		Format:     strings.ToLower(filepath.Ext(filePath)[1:]), // Убираем точку
		Size:       fileInfo.Size(),
		Duration:   metadata.Duration,
		HasVideo:   metadata.HasVideo,
		HasAudio:   metadata.HasAudio,
		VideoCodec: metadata.VideoCodec,
		AudioCodec: metadata.AudioCodec,
		SampleRate: metadata.SampleRate,
		Channels:   metadata.Channels,
		Width:      metadata.Width,
		Height:     metadata.Height,
	}

	// Проверяем поддержку формата
	if !IsSupportedFormat(mediaFile.Format) {
		return nil, fmt.Errorf("unsupported format: %s", mediaFile.Format)
	}

	return mediaFile, nil
}

// ExtractCover извлекает обложку из медиа-файла
func (f *FFmpegProcessor) ExtractCover(mediaFile *MediaFile) (string, error) {
	if !mediaFile.HasVideo {
		return "", fmt.Errorf("no video stream to extract cover from")
	}

	outputPath := filepath.Join(f.tempDir, fmt.Sprintf("cover_%s.jpg", mediaFile.Name))

	// Извлекаем первый кадр как обложку
	cmd := exec.Command(f.ffmpegPath,
		"-i", mediaFile.Path,
		"-vframes", "1",
		"-q:v", "2",
		"-y", // Перезаписывать существующий файл
		outputPath,
	)

	if err := cmd.Run(); err != nil {
		return "", NewFFmpegError("extract cover", cmd, err)
	}

	mediaFile.CoverPath = outputPath
	return outputPath, nil
}

// ExtractPosterFrame извлекает первый кадр видео
func (f *FFmpegProcessor) ExtractPosterFrame(mediaFile *MediaFile) (string, error) {
	if !mediaFile.HasVideo {
		return "", fmt.Errorf("no video stream to extract poster frame from")
	}

	outputPath := filepath.Join(f.tempDir, fmt.Sprintf("poster_%s.jpg", mediaFile.Name))

	cmd := exec.Command(f.ffmpegPath,
		"-i", mediaFile.Path,
		"-vframes", "1",
		"-q:v", "2",
		"-y",
		outputPath,
	)

	if err := cmd.Run(); err != nil {
		return "", NewFFmpegError("extract poster frame", cmd, err)
	}

	mediaFile.PosterFrame = outputPath
	return outputPath, nil
}

// GenerateWaveform генерирует волнограмму аудио
func (f *FFmpegProcessor) GenerateWaveform(mediaFile *MediaFile) (string, error) {
	if !mediaFile.HasAudio {
		return "", fmt.Errorf("no audio stream to generate waveform from")
	}

	outputPath := filepath.Join(f.tempDir, fmt.Sprintf("waveform_%s.png", mediaFile.Name))

	// Генерируем простую волнограмму
	cmd := exec.Command(f.ffmpegPath,
		"-i", mediaFile.Path,
		"-filter_complex", "showwavespic=s=1200x200:colors=gray",
		"-frames:v", "1",
		"-y",
		outputPath,
	)

	if err := cmd.Run(); err != nil {
		return "", NewFFmpegError("generate waveform", cmd, err)
	}

	mediaFile.WaveformPath = outputPath
	return outputPath, nil
}

// OptimizeVideoForFastStart оптимизирует видео для быстрого старта (faststart)
func (f *FFmpegProcessor) OptimizeVideoForFastStart(inputPath, outputPath string) error {
	if !strings.HasSuffix(strings.ToLower(inputPath), ".mp4") {
		return fmt.Errorf("faststart optimization only supported for MP4 files")
	}

	cmd := exec.Command(f.ffmpegPath,
		"-i", inputPath,
		"-c", "copy", // Копируем без перекодирования
		"-movflags", "+faststart", // Перемещаем moov-бокс в начало
		"-y", // Перезаписывать существующий файл
		outputPath,
	)

	if err := cmd.Run(); err != nil {
		return NewFFmpegError("optimize video for faststart", cmd, err)
	}

	return nil
}

// ReencodeWithFrequentKeyframes перекодирует видео с частыми ключевыми кадрами
func (f *FFmpegProcessor) ReencodeWithFrequentKeyframes(inputPath, outputPath string) error {
	cmd := exec.Command(f.ffmpegPath,
		"-i", inputPath,
		"-c:v", "libx264", // Видеокодек H.264
		"-g", "48", // GOP size = 48 кадров (≈ 2 сек при 24 fps)
		"-keyint_min", "48", // Минимальный интервал ключевых кадров
		"-sc_threshold", "0", // Отключаем сценарные изменения
		"-c:a", "copy", // Копируем аудио без перекодирования
		"-preset", "fast", // Быстрое кодирование
		"-crf", "23", // Качество (18-28, чем меньше тем лучше)
		"-y", // Перезаписывать существующий файл
		outputPath,
	)

	if err := cmd.Run(); err != nil {
		return NewFFmpegError("reencode with frequent keyframes", cmd, err)
	}

	return nil
}

// OptimizeVideoWithHardwareAcceleration оптимизирует видео с аппаратным ускорением
func (f *FFmpegProcessor) OptimizeVideoWithHardwareAcceleration(inputPath, outputPath string) error {
	// Определяем доступное аппаратное ускорение
	var hwAccel string
	var hwEncoder string

	// Проверяем доступность различных аппаратных ускорителей
	if f.checkHardwareAcceleration("videotoolbox") {
		hwAccel = "videotoolbox"
		hwEncoder = "h264_videotoolbox"
	} else if f.checkHardwareAcceleration("cuda") {
		hwAccel = "cuda"
		hwEncoder = "h264_nvenc"
	} else if f.checkHardwareAcceleration("vaapi") {
		hwAccel = "vaapi"
		hwEncoder = "h264_vaapi"
	} else {
		// Если аппаратное ускорение недоступно, используем программное
		return f.ReencodeWithFrequentKeyframes(inputPath, outputPath)
	}

	cmd := exec.Command(f.ffmpegPath,
		"-hwaccel", hwAccel,
		"-i", inputPath,
		"-c:v", hwEncoder,
		"-g", "48",
		"-keyint_min", "48",
		"-sc_threshold", "0",
		"-c:a", "copy",
		"-y",
		outputPath,
	)

	if err := cmd.Run(); err != nil {
		return NewFFmpegError("optimize video with hardware acceleration", cmd, err)
	}

	return nil
}

// checkHardwareAcceleration проверяет доступность аппаратного ускорения
func (f *FFmpegProcessor) checkHardwareAcceleration(accelType string) bool {
	cmd := exec.Command(f.ffmpegPath, "-hide_banner", "-hwaccels")
	output, err := cmd.Output()
	if err != nil {
		return false
	}

	return strings.Contains(string(output), accelType)
}

// GetVideoOptimizationInfo возвращает информацию о возможностях оптимизации
func (f *FFmpegProcessor) GetVideoOptimizationInfo() map[string]bool {
	info := make(map[string]bool)

	// Проверяем поддержку различных аппаратных ускорителей
	info["videotoolbox"] = f.checkHardwareAcceleration("videotoolbox")
	info["cuda"] = f.checkHardwareAcceleration("cuda")
	info["vaapi"] = f.checkHardwareAcceleration("vaapi")

	return info
}

// Metadata содержит метаданные медиа-файла
type Metadata struct {
	Duration   time.Duration
	HasVideo   bool
	HasAudio   bool
	VideoCodec string
	AudioCodec string
	SampleRate int
	Channels   int
	Width      int
	Height     int
}

// extractMetadata извлекает метаданные через ffprobe
func (f *FFmpegProcessor) extractMetadata(filePath string) (*Metadata, error) {
	// Используем ffprobe для получения метаданных
	cmd := exec.Command("ffprobe",
		"-v", "quiet",
		"-print_format", "json",
		"-show_format",
		"-show_streams",
		filePath,
	)

	output, err := cmd.Output()
	if err != nil {
		return nil, NewFFmpegError("extract metadata", cmd, err)
	}

	// Парсим JSON
	return f.parseFFprobeOutput(string(output))
}

// parseFFprobeOutput парсит вывод ffprobe
func (f *FFmpegProcessor) parseFFprobeOutput(output string) (*Metadata, error) {
	var ffprobeOutput FFprobeOutput
	if err := json.Unmarshal([]byte(output), &ffprobeOutput); err != nil {
		return nil, fmt.Errorf("failed to parse ffprobe JSON: %w", err)
	}

	metadata := &Metadata{}

	// Парсим длительность
	if ffprobeOutput.Format.Duration != "" {
		if duration, err := strconv.ParseFloat(ffprobeOutput.Format.Duration, 64); err == nil {
			metadata.Duration = time.Duration(duration * float64(time.Second))
		}
	}

	// Анализируем потоки
	for _, stream := range ffprobeOutput.Streams {
		switch stream.CodecType {
		case "video":
			metadata.HasVideo = true
			metadata.VideoCodec = stream.CodecName
			metadata.Width = stream.Width
			metadata.Height = stream.Height
		case "audio":
			metadata.HasAudio = true
			metadata.AudioCodec = stream.CodecName
			metadata.Channels = stream.Channels
			if stream.SampleRate != "" {
				if sampleRate, err := strconv.Atoi(stream.SampleRate); err == nil {
					metadata.SampleRate = sampleRate
				}
			}
		}
	}

	return metadata, nil
}

// Cleanup очищает временные файлы
func (f *FFmpegProcessor) Cleanup() error {
	return os.RemoveAll(f.tempDir)
}
